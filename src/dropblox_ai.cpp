#include "dropblox_ai.h"

using namespace json;
using namespace std;

//----------------------------------
// Block implementation starts here!
//----------------------------------

Block::Block(Object& raw_block) {
  center.i = (int)(Number&)raw_block["center"]["i"];
  center.j = (int)(Number&)raw_block["center"]["j"];
  size = 0;

  Array& raw_offsets = raw_block["offsets"];
  for (Array::const_iterator it = raw_offsets.Begin(); it < raw_offsets.End(); it++) {
    size += 1;
  }
  for (int i = 0; i < size; i++) {
    offsets[i].i = (Number&)raw_offsets[i]["i"];
    offsets[i].j = (Number&)raw_offsets[i]["j"];
  }

  translation.i = 0;
  translation.j = 0;
  rotation = 0;
}

void Block::left() {
  translation.j -= 1;
}

void Block::right() {
  translation.j += 1;
}

void Block::up() {
  translation.i -= 1;
}

void Block::down() {
  translation.i += 1;
}

void Block::rotate() {
  rotation += 1;
}

void Block::unrotate() {
  rotation -= 1;
}

// The checked_* methods below perform an operation on the block
// only if it's a legal move on the passed in board.  They
// return true if the move succeeded.
//
// The block is still assumed to start in a legal position.
bool Block::checked_left(const Board& board) {
  left();
  if (board.check(*this)) {
    return true;
  }
  right();
  return false;
}

bool Block::checked_right(const Board& board) {
  right();
  if (board.check(*this)) {
    return true;
  }
  left();
  return false;
}

bool Block::checked_up(const Board& board) {
  up();
  if (board.check(*this)) {
    return true;
  }
  down();
  return false;
}

bool Block::checked_down(const Board& board) {
  down();
  if (board.check(*this)) {
    return true;
  }
  up();
  return false;
}

bool Block::checked_rotate(const Board& board) {
  rotate();
  if (board.check(*this)) {
    return true;
  }
  unrotate();
  return false;
}

void Block::do_command(const string& command) {
  if (command == "left") {
    left();
  } else if (command == "right") {
    right();
  } else if (command == "up") {
    up();
  } else if (command == "down") {
    down();
  } else if (command == "rotate") {
    rotate();
  } else {
    throw Exception("Invalid command " + command);
  }
}

void Block::do_commands(const vector<string>& commands) {
  for (int i = 0; i < commands.size(); i++) {
    do_command(commands[i]);
  }
}

void Block::reset_position() {
  translation.i = 0;
  translation.j = 0;
  rotation = 0;
}

//----------------------------------
// Board implementation starts here!
//----------------------------------

Board::Board() {
  rows = ROWS;
  cols = COLS;
}

Board::Board(Object& state) {
  rows = ROWS;
  cols = COLS;

  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      bitmap[i][j] = ((int)(Number&)state["bitmap"][i][j] ? 1 : 0);
    }
  }

  // Note that these blocks are NEVER destructed! This is because calling
  // place() on a board will create new boards which share these objects.
  //
  // There's a memory leak here, but it's okay: blocks are only constructed
  // when you construct a board from a JSON Object, which should only happen
  // for the very first board. The total memory leaked will only be ~10 kb.
  block = new Block(state["block"]);
  for (int i = 0; i < PREVIEW_SIZE; i++) {
    preview.push_back(new Block(state["preview"][i]));
  }
}

// Returns true if the `query` block is in valid position - that is, if all of
// its squares are in bounds and are currently unoccupied.
bool Board::check(const Block& query) const {
  Point point;
  for (int i = 0; i < query.size; i++) {
    point.i = query.center.i + query.translation.i;
    point.j = query.center.j + query.translation.j;
    if (query.rotation % 2) {
      point.i += (2 - query.rotation)*query.offsets[i].j;
      point.j +=  -(2 - query.rotation)*query.offsets[i].i;
    } else {
      point.i += (1 - query.rotation)*query.offsets[i].i;
      point.j += (1 - query.rotation)*query.offsets[i].j;
    }
    if (point.i < 0 || point.i >= ROWS ||
        point.j < 0 || point.j >= COLS || bitmap[point.i][point.j]) {
      return false;
    }
  }
  return true;
}

vector<string> Board::find_path(Block* start, Block* end){
	vector<string> commands;
	while(end.translation != start.translation){
		while(end.translation.j != start.translation.j){
			end.up();
			if(check(end)){
				commands.push_back("down");
			}
			else{
				end.down();
				end.right();
				if(check(end)){
					commands.push_back("left");
				}
				else{
					end.left();
					end.left();
					if(check(end)){
						commands.push_back("right");
					}
					else{
						end.right();
						end.rotate();
						commands.push_back("rotate");
						int rot = 1;
						while(!check(end)){
							end.rotate();
							rot++;
							if(rot >= 4){
								return null;
							}
							else{
								commands.push_back("rotate");
							}
						}
					}
				}
			}
		}
		if(end.translation.i > start.translation.i){
			end.left();
			commands.push_back("right");
		}
		else if(end.translation.i < start.translation.i){
			end.right();
			commands.push_back("left");
		}
	}
	while(end.rotate%4 != start.rotate%4){
		end.rotate();
		commands.push_back("rotate");
	}
	
	vector<string> final_commands;
	for(int i = commands.size()-1; i > 0; i--){
		final_commands.push_back(commands.at(i));
	}
	
	return final_commands;
}

// Resets the block's position, moves it according to the given commands, then
// drops it onto the board. Returns a pointer to the new board state object.
//
// Throws an exception if the block is ever in an invalid position.
Board* Board::do_commands(const vector<string>& commands) {
  block->reset_position();
  if (!check(*block)) {
    throw Exception("Block started in an invalid position");
  }
  for (int i = 0; i < commands.size(); i++) {
    if (commands[i] == "drop") {
      return place();
    } else {
      block->do_command(commands[i]);
      if (!check(*block)) {
        throw Exception("Block reached in an invalid position");
      }
    }
  }
  // If we've gotten here, there was no "drop" command. Drop anyway.
  return place();
}

// Drops the block from whatever position it is currently at. Returns a
// pointer to the new board state object, with the next block drawn from the
// preview list.
//
// Assumes the block starts out in valid position.
// This method translates the current block downwards.
//
// If there are no blocks left in the preview list, this method will fail badly!
// This is okay because we don't expect to look ahead that far.
Board* Board::place() {
  Board* new_board = new Board();

  while (check(*block)) {
    block->down();
  }
  block->up();

  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      new_board->bitmap[i][j] = bitmap[i][j];
    }
  }

  Point point;
  for (int i = 0; i < block->size; i++) {
    point.i = block->center.i + block->translation.i;
    point.j = block->center.j + block->translation.j;
    if (block->rotation % 2) {
      point.i += (2 - block->rotation)*block->offsets[i].j;
      point.j +=  -(2 - block->rotation)*block->offsets[i].i;
    } else {
      point.i += (1 - block->rotation)*block->offsets[i].i;
      point.j += (1 - block->rotation)*block->offsets[i].j;
    }
    new_board->bitmap[point.i][point.j] = 1;
  }
  Board::remove_rows(&(new_board->bitmap));

  new_board->block = preview[0];
  for (int i = 1; i < preview.size(); i++) {
    new_board->preview.push_back(preview[i]);
  }

  return new_board;
}

// A static method that takes in a new_bitmap and removes any full rows from it.
// Mutates the new_bitmap in place.
void Board::remove_rows(Bitmap* new_bitmap) {
  int rows_removed = 0;
  for (int i = ROWS - 1; i >= 0; i--) {
    bool full = true;
    for (int j = 0; j < COLS; j++) {
      if (!(*new_bitmap)[i][j]) {
        full = false;
        break;
      }
    }
    if (full) {
      rows_removed += 1;
    } else if (rows_removed) {
      for (int j = 0; j < COLS; j++) {
        (*new_bitmap)[i + rows_removed][j] = (*new_bitmap)[i][j];
      }
    }
  }
  for (int i = 0; i < rows_removed; i++) {
    for (int j = 0; j < COLS; j++) {
      (*new_bitmap)[i][j] = 0;
    }
  }
}

int Board::rankAll(Block* block){
	Bitmap* new_bitmap = &bitmap;
	for(i = 0; i < block.offsets.size(); i++){
		Point p = block.offsets[i];
		new_bitmap[p.i][p.j] = 1;
	}
	rankLine(new_bitmap);
	rankHole(new_bitmap);
	rankFlat(new_bitmap);
	rankHeight(new_bitmap);
	rankFuture(new_bitmap);
}

//complete lines and partial lines
int Board::rankLine(Bitmap* map){
	int SCORE_LINE = 100;
	int SCORE_PARTIAL = 1;
	int line_score;
	int final_score = 0;
	for(int i = 0; i < ROWS; i++){
		line_score = 0;
		for(int j = 0; j < COLS; j++){
			if(map[i][j] == 1){
				line_score++;
			}
		}
		if(line_score == COLS){
			final_score += SCORE_LINE;
		}
		else{
			final_score += SCORE_PARTIAL;
		}
	}
	return final_score;
}


int Board::rankHole(Bitmap* map){
	int SCORE_OPEN = -10;
	int SCORE_CLOSED = - 100;
	int SCORE_FILL = 25;
	int final_score = 0;
	for(int j = 0; j < COLS; j++){
		int isHole = 0;
		for(int i = 0; i < ROWS; i++){
			if(map[i][j] == 0){
				isHole += 1;
				if(map[max(0,i-1)][j] == 1 && map[min(ROWS,i+1)][j] == 1 && map[i][min(COLS,j+1)] == 1 && map[i][max(0,j-1)] == 1){
					final_score += SCORE_CLOSED;
					break;
				}
			}
			if(map[i][j] == 1){
				if(isHole > 0){
					final_score += SCORE_OPEN * min(10, isHole);
					isHole = 0;
				}
			}
		}
	}
}

int Board::rankFlat(Bitmap* map) {
  int SCORE_MULTIPLIER = -10;

  float slope = 0;
  float slopeChange = 0;
  for (int i = 0; i < cols - 3; i++) {
    float a, b, c, d;
    for (int j = rows-1; j > 0; j--) {
      if (map[j][i]) {
        a = j;
      }
    }
    for (int j = rows-1; j > 0; j--) {
      if (map[j][i + 1]) {
        b = j;
      }
    }
    for (int j = rows-1; j > 0; j--) {
      if (map[j][i + 2]) {
        c = j;
      }
    }
    for (int j = rows-1; j > 0; j--) {
      if (map[j][i + 3]) {
        d = j;
      }
    }

    float temp = b - a + c - a + d - a;
    temp /= 4.f;
    slopeChange += Math.abs(slope - temp);
    slope = temp;
  }
  // score based on slopeChange.
  return slopeChange * SCORE_MULTIPLIER;
}

int Board::rankHeight(Bitmap* map);
int Board::rankFuture(Bitmap* map);

struct ScoredBlock {
  Block* b;
  int score;
  vector<String> path;
};

int main(int argc, char** argv) {
  // Construct a JSON Object with the given game state.
  istringstream raw_state(argv[1]);
  Object state;
  Reader::Read(state, raw_state);

  // Construct a board from this Object.
  Board board(state);
/*
  vector<Block*> possibilities = board.checkAllPositions(*board.block);
  vector<ScoredBlock> scoredBlocks();
  for (int i = 0; i < possibilities.size(); i++) {
    ScoredBlock b;
    b.b = possiblies[i];
    b.score = rankAll(possiblies[i]);
    b.path = find_path(board.block, possiblies[i]);
  }
  
  // Make some moves!
  vector<string> moves;
  while (board.check(*board.block)) {
    board.block->left();
    moves.push_back("left");
  }*/

  int pos =   Math.rand()%board.rows - board.rows/2;
  for (int i = 0; i < Math.abs(pos); i++) {
    if (pos < 0) {

      cout << "left" << endl;
    } else {
      cout << "right" << endl;
    }
  }
  cout << "place" << endl;
  // Ignore the last move, because it moved the block into invalid
  // position. Make all the rest.

}

vector <Block*> Board::checkAllPositions(Block baseBlock) {
  // scan bitmap from bottom up
  // try placing the block's center in each of the possible spaces until you pass the nth completely unoccupied row, where n is the radius of the block.
  int radius = 0;

  for (int i = 0; i < baseBlock.size; i++) {
    if (Math.abs(offsets[i].i) > radius) {
      radius = Math.abs(offsets[i].i);
    } if (Math.abs(offsets[i].j) > radius) {
      radius = Math.abs(offsets[i].j);
    }
  }
  vector <Block*> goodBlocks = new vector<Block*>();

  int emptyRowCount = 0;
  bool emptyRow = true;
  for (int i = 1; i < board.rows; i++) {
    for (int j = 0; j < board.cols; j++) {
      if (board.bitmap[board.rows - i][j] == 0) {
        Point p;
        p.i = i;
        p.j = j;
        Block * b new Block(baseBlock);
        b->translation = p;
        if (check(b)) {
          goodBlocks->push_back(b);
        }
      } else {
        emptyRow = false;
      }
    }
    if (emptyRow) {
      emptyRowCount++;
      emptyRow = true;
    }
    if (emptyRowCount > radius) {
      break;
    }
  }

  return goodBlocks;

}
