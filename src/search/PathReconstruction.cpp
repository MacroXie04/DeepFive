#include "PathReconstruction.h"
#include <algorithm>

std::vector<Move> reconstructPath(ForcedNode* leaf) {
    std::vector<Move> path;
    ForcedNode* current = leaf;
    while (current != nullptr && current->parent != nullptr) {
        path.push_back(current->move);
        current = current->parent;
    }
    std::reverse(path.begin(), path.end());
    return path;
}
