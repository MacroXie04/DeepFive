#include "heuristics.h"
#include <algorithm>
#include <random>
#include <thread>

namespace Heuristics {

    // Thread-local random generator
    static thread_local std::mt19937 rng(std::random_device{}());

    std::vector<Move> getCandidateMoves(const Board& board, Player side) {
        std::vector<Move> moves;
        moves.reserve(50); 
        int size = board.size();
        
        // Optimization: Only scan relevant area
        int minR = size, maxR = -1, minC = size, maxC = -1;
        bool empty = true;
        for(int r=0; r<size; ++r) {
            for(int c=0; c<size; ++c) {
                if (!board.isEmpty(r,c)) {
                    empty = false;
                    minR = std::min(minR, r);
                    maxR = std::max(maxR, r);
                    minC = std::min(minC, c);
                    maxC = std::max(maxC, c);
                }
            }
        }
        
        if (empty) {
            moves.push_back({size/2, size/2, side});
            return moves;
        }
        
        // Expand bounds by 2
        minR = std::max(0, minR - 2);
        maxR = std::min(size-1, maxR + 2);
        minC = std::max(0, minC - 2);
        maxC = std::min(size-1, maxC + 2);

        for (int r = minR; r <= maxR; ++r) {
            for (int c = minC; c <= maxC; ++c) {
                if (board.isEmpty(r, c)) {
                    // Check if it has neighbor within 2
                    bool hasNeighbor = false;
                    for (int dr = -2; dr <= 2 && !hasNeighbor; ++dr) {
                        for (int dc = -2; dc <= 2; ++dc) {
                             int nr = r + dr;
                             int nc = c + dc;
                             if (board.isInside(nr, nc) && !board.isEmpty(nr, nc)) {
                                 hasNeighbor = true;
                                 break;
                             }
                        }
                    }
                    if (hasNeighbor) {
                        moves.push_back({r, c, side});
                    }
                }
            }
        }

        return moves;
    }

    std::optional<Move> getFastRandomMove(const Board& board, Player side) {
        int size = board.size();
        
        static thread_local std::vector<std::pair<int,int>> stones;
        stones.clear();
        stones.reserve(225);
        
        for(int r=0; r<size; ++r) {
            for(int c=0; c<size; ++c) {
                if(!board.isEmpty(r,c)) {
                    stones.push_back({r,c});
                }
            }
        }
        
        if (stones.empty()) {
            return Move{size/2, size/2, side};
        }
        
        std::shuffle(stones.begin(), stones.end(), rng);
        
        // Try neighbors of random stones
        for(const auto& s : stones) {
            int r = s.first;
            int c = s.second;
            
            // Randomize neighbor order
            int drs[] = {-1, -1, -1, 0, 0, 1, 1, 1};
            int dcs[] = {-1, 0, 1, -1, 1, -1, 0, 1};
            int indices[] = {0,1,2,3,4,5,6,7};
            
            // Partial shuffle of indices
            for (int i = 0; i < 8; ++i) {
                 int j = std::uniform_int_distribution<>(i, 7)(rng);
                 std::swap(indices[i], indices[j]);
            }
            
            for(int idx : indices) {
                int nr = r + drs[idx];
                int nc = c + dcs[idx];
                if(board.isInside(nr, nc) && board.isEmpty(nr, nc)) {
                    return Move{nr, nc, side};
                }
            }
        }
        
        return std::nullopt;
    }

    std::optional<Move> checkImmediateThreats(const Board& board, Player side) {
        Player opp = (side == Player::Black) ? Player::White : Player::Black;
        auto candidates = getCandidateMoves(board, side);
        
        // 1. Can we win directly?
        for (const auto& move : candidates) {
            Board temp = board;
            temp.placeStone(move.row, move.col, side);
            if (temp.checkWinner() == side) {
                return move;
            }
        }

        // 2. Must we block opponent win (Connect 4)?
        for (const auto& move : candidates) {
            Board temp = board;
            temp.placeStone(move.row, move.col, opp);
            if (temp.checkWinner() == opp) {
                return Move{move.row, move.col, side}; // Block it!
            }
        }
        
        for (const auto& move : candidates) {
            Board temp = board;
            temp.placeStone(move.row, move.col, opp);
            
            int r = move.row;
            int c = move.col;
            int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
            
            for (auto& dir : directions) {
                int count = 1;
                // Check forward
                int i = 1;
                while (temp.isInside(r + i*dir[0], c + i*dir[1]) && temp.at(r + i*dir[0], c + i*dir[1]) == opp) {
                    count++;
                    i++;
                }
                bool openFront = temp.isInside(r + i*dir[0], c + i*dir[1]) && temp.isEmpty(r + i*dir[0], c + i*dir[1]);
                
                // Check backward
                int j = 1;
                while (temp.isInside(r - j*dir[0], c - j*dir[1]) && temp.at(r - j*dir[0], c - j*dir[1]) == opp) {
                    count++;
                    j++;
                }
                bool openBack = temp.isInside(r - j*dir[0], c - j*dir[1]) && temp.isEmpty(r - j*dir[0], c - j*dir[1]);
                
                if (count == 4 && openFront && openBack) {
                    return Move{move.row, move.col, side};
                }
                 if (count == 4 && (openFront || openBack)) {
                    return Move{move.row, move.col, side};
                }
            }
        }

        return std::nullopt;
    }

}
