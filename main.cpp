#include <iostream>
#include <cstdlib>
#include "raylib.h"
#include <map>
#include <vector>
#include <ctime>   // For time()

using namespace std;


// TOFIX:
// - Make ant walk back to home not TP
// - Turn ant spawns on when everything else works
// - Figure out why FindPathHome goes into infinite loop sometimes and crashes
// - Lay down food trail
// - Ideally, add checking for home/food trail in findpathhome, right now any grid cell type can be traversed to go home
// - Darker squares getting lighter after walking over them


enum AntState {
    LookingForFood,
    LookingForHome,
    RunningFromPredator
};

enum AntSpecies {
    Fire,
    Bullet,
    WaspKiller
};

enum AntType {
    Worker,
    Solider,
    Builder,
    Gatherer
};

enum CellType {
    Obstacle,
    AntHome,
    Food,
    HomeTrail,
    FoodTrail,
    PredatorTrail
};

map<CellType, Color> cellToColorMap = { {CellType::Obstacle, BROWN}, {CellType::AntHome, YELLOW}, {CellType::Food, GREEN}, {CellType::HomeTrail, DARKPURPLE}, {CellType::FoodTrail, ORANGE}, {CellType::PredatorTrail, RED}};

class GridCell {
public:
    CellType cellType;
    Color cellColor;

    GridCell() : cellType(CellType::Obstacle), cellColor(WHITE) {} // Initialize with default values

    GridCell(CellType _cellType) {
        this->cellType = _cellType;

        this->cellColor = cellToColorMap[this->cellType];
    }
};

bool AreColorsSame(Color color1, Color color2) {
    if (color1.r == color2.r && color1.g == color2.g && color1.b == color2.b) {
        return true;
    }
    return false;
}

// Function to draw the grid lines
void DrawGrid(int gridSize, int screenWidth, int screenHeight) {
    for (int x = 0; x <= screenWidth; x += gridSize) {
        DrawLine(x, 0, x, screenHeight, DARKGRAY);
    }
    for (int y = 0; y <= screenHeight; y += gridSize) {
        DrawLine(0, y, screenWidth, y, DARKGRAY);
    }
}

// Function to draw the grid cells
void DrawGridCells(int gridSize, int screenWidth, int screenHeight, const std::map<std::pair<int, int>, GridCell>& gridMap) {
    for (int x = 0; x <= screenWidth; x += gridSize) {
        for (int y = 0; y <= screenHeight; y += gridSize) {
            int cellX = x / gridSize;
            int cellY = y / gridSize;

            std::pair<int, int> cellKey = { cellX, cellY };

            Color cellColor = BLACK;

            auto it = gridMap.find(cellKey);
            if (it != gridMap.end()) {
                cellColor = it->second.cellColor; // cell color stored in grid state
            }

            DrawRectangle(x, y, gridSize, gridSize, cellColor);
        }
    }
}

void DrawOverGridCells(int gridSize, std::map<std::pair<int, int>, GridCell>& gridMap, CellType &currentlySelectedCellType) {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 mousePosition = GetMousePosition(); // pos of selected cell
        int cellCol = static_cast<int>(mousePosition.x) / gridSize; // which col is the cell in
        int cellRow = static_cast<int>(mousePosition.y) / gridSize; // which row is the cell in

        // check if you're drawing over ant home - Not allowed
        auto it = gridMap.find({ cellCol, cellRow });
        if (it != gridMap.end()) {
            if (it->second.cellType == CellType::AntHome) {
                return;
            }
        }

        gridMap[{cellCol, cellRow}] = GridCell(currentlySelectedCellType);
    }
}

void ChangeDrawingCellType(CellType& selectedCellType) {
    if (IsKeyPressed(KEY_ONE)) {
        selectedCellType = CellType::Obstacle;
    }
    else if (IsKeyPressed(KEY_TWO)) {
        selectedCellType = CellType::AntHome;
    }
    else if (IsKeyPressed(KEY_THREE)) {
        selectedCellType = CellType::Food;
    }
    else if (IsKeyPressed(KEY_FOUR)) {
        selectedCellType = CellType::HomeTrail;
    }
    else if (IsKeyPressed(KEY_FIVE)) {
        selectedCellType = CellType::FoodTrail;
    }
    else if (IsKeyPressed(KEY_SIX)) {
        selectedCellType = CellType::PredatorTrail;
    }
}

vector<int> ConvertGridCellLocationToCoordinates(int gridSize, int cellCol, int cellRow) { // maps {cellCol, cellRow} -> {x, y}
    return { cellCol * gridSize, cellRow * gridSize }; // x, y coords
}

class Ant {
public:
    int x, y;
    int speedX, speedY;
    AntState antState;
    AntSpecies antSpecies;
    AntType antType;

    Ant(int _x, int _y, int _speedX, int _speedY, AntState _antState, AntSpecies _antSpecies, AntType _antType) {
        this->x = _x;
        this->y = _y;
        this->speedX = _speedX;
        this->speedY = _speedY;
        this->antState = _antState;
        this->antSpecies = _antSpecies;
        this->antType = _antType;

        this->currentDirection = rand() % 8;
        this->changeDirectionTimer = 0;
        this->changeDirectionInterval = 2;
    }

    // HORRIBLE, NEEDS IMPROVEMENT -> needs to actually move towards nearest trail.
    // Need to also lay down food trial once food is reached until you're at home then lay down home trail again
    
    vector<pair<int, int>> FindPathHome(std::map<std::pair<int, int>, GridCell>& gridMap, pair<int, int> currCellLoc, vector<pair<int, int>> visitedPaths = {}) {
        
        //cout << "Find Path Home" << endl;
        
        vector<pair<int, int>> finalPath = {};

        if (gridMap.find(currCellLoc) == gridMap.end()) {
            // Current cell not found in gridMap
            return finalPath; // Return an empty vector as path
        }

        if (gridMap[currCellLoc].cellType == CellType::AntHome) {
            // Current cell is AntHome, return visitedPaths
            return visitedPaths;
        }

        for (int i = -1; i < 2; i++) {
            for (int j = -1; j < 2; j++) {
                if (i == 0 && j == 0) {
                    continue;
                }

                pair<int, int> newCellLoc = { currCellLoc.first + i, currCellLoc.second + j };

                // Check if new cell location is within gridMap
                if (gridMap.find(newCellLoc) != gridMap.end()) {
                    // Check if new cell location has been visited before
                    if (find(visitedPaths.begin(), visitedPaths.end(), newCellLoc) == visitedPaths.end() && gridMap[newCellLoc].cellType != Food) {
                        // TEST: Ensure only home or food trails can be traversed!
                        //cout << "Not Food and Not Visited" << endl;
                        // Clone visitedPaths and append newCellLoc to it
                        vector<pair<int, int>> newVisitedPaths = visitedPaths;
                        newVisitedPaths.push_back(newCellLoc);

                        // Recursively find path from newCellLoc
                        finalPath = FindPathHome(gridMap, newCellLoc, newVisitedPaths);
                        if (!finalPath.empty()) {
                            // Path found, return the final path
                            return finalPath;
                        }
                    }
                }
            }
        }

        return finalPath; // Return an empty vector if no path found
    }

    void FollowPathHome(int gridSize, std::map<std::pair<int, int>, GridCell>& gridMap, vector<pair<int, int>>& path) {
        // Lay down food trail as it walks back home.
        // when another ant thats looking for food stumbles upon a food trail, they follow it unless they are looking for home.
        // food trails should not be overriden by home trail
        // food trails can also be reversed to go back home like home trails. They act as 2 for 1 (can lead to food and back to home).
        
        cout << "Follow Path Home" << endl;

        if (path.empty()) {
            this->antState = AntState::LookingForFood;
            return; // No path to follow
        }

        // This instantly TP's ant to home, I want it to walk back home.

        for (int i = 0; i < path.size(); i++) {
            pair<int, int> cellToMoveTo = { path[i].first, path[i].second };

            this->x = std::ceil(cellToMoveTo.first * gridSize);
            this->y = std::ceil(cellToMoveTo.second * gridSize);

            if (i == path.size() - 1) {
                this->antState = AntState::LookingForFood;
                return;
            }
        }
    }

    void DrawPathHome(int gridSize, const vector<pair<int, int>>& path) {

        cout << "Draw Path" << endl;

        for (size_t i = 0; i < path.size() - 1; ++i) {
            // Calculate coordinates of current cell
            int startX = path[i].first * gridSize + gridSize / 2;
            int startY = path[i].second * gridSize + gridSize / 2;

            // Calculate coordinates of next cell
            int endX = path[i + 1].first * gridSize + gridSize / 2;
            int endY = path[i + 1].second * gridSize + gridSize / 2;

            // Draw line between current cell and next cell
            DrawLine(startX, startY, endX, endY, BLUE);
        }
    }

    void TraverseHomePheromone(int gridSize, std::map<std::pair<int, int>, GridCell>& gridMap, int screenWidth, int screenHeight) {
        
        cout << "Traverse Home -------------------------" << endl;

        std::pair<int, int> currentCell = CurrentCellAntIsOn(gridSize);
        std::pair<int, int> closestCell = currentCell;
        bool trailFound = false;

        // follow pre-determined home path here.
        vector<pair<int, int>> pathHome = FindPathHome(gridMap, currentCell);

        if (!pathHome.empty()) {
            cout << "Path Not Empty!" << endl;
            DrawPathHome(gridSize, pathHome);
            FollowPathHome(gridSize, gridMap, pathHome);
        }
        else {
            cout << "Path Empty!" << endl;
            this->antState = AntState::LookingForFood;
            this->MoveRandomly(screenWidth, screenHeight, gridSize, gridMap);
        }

        return;
    }

    void MoveRandomly(int screenWidth, int screenHeight, int gridSize, std::map<std::pair<int, int>, GridCell>& gridMap) {
        
        
        ChangeDirection(GetFrameTime());
        
        
        // Move ant randomly within the screen bounds
        switch (this->currentDirection) {
        case 0: // Move up
            y -= speedY;
            break;
        case 1: // Move down
            y += speedY;
            break;
        case 2: // Move left
            x -= speedX;
            break;
        case 3: // Move right
            x += speedX;
            break;
        case 4: // up left
            y -= speedY;
            x -= speedX;
            break;
        case 5: // up right
            y -= speedY;
            x += speedX;
            break;
        case 6: // down left
            y += speedY;
            x -= speedX;
            break;
        case 7: // down right
            y += speedY;
            x += speedX;
            break;
        }


        // Ensure ant stays within screen bounds

        pair<int, int> currentCell = CurrentCellAntIsOn(gridSize);

        // moving randomly but looking for home
        if (this->antState == AntState::LookingForHome) {
            for (int dx = -1; dx < 2; ++dx) {
                for (int dy = -1; dy < 2; ++dy) {
                    if (dx == 0 && dy == 0) continue; // Skip the current cell

                    pair<int, int> neighbourCell = { currentCell.first + dx, currentCell.second + dy };
                    auto it = gridMap.find(neighbourCell);
                    if (it != gridMap.end()) {
                        if (it->second.cellType == CellType::HomeTrail || it->second.cellType == CellType::AntHome) {
                            this->TraverseHomePheromone(gridSize, gridMap, screenWidth, screenHeight); // trail or home found, traverse back to home
                        }
                    }
                }
            }
        }

        auto it = gridMap.find(currentCell);
        
        if (it != gridMap.end()) { // current cell is in map

            if (it->second.cellType == CellType::Food) {
                if (this->antState == AntState::LookingForFood) {
                    this->antState = AntState::LookingForHome;
                }
            }

            if (it->second.cellType != CellType::AntHome && it->second.cellType != CellType::Food) { // Don't add pheromones over spawn / food
                Color cellColor = it->second.cellColor;

                if (it->second.cellType == CellType::Obstacle) { // obstacle
                    this->currentDirection = rand() % 8;
                }
                
                if (it->second.cellType == CellType::HomeTrail) { // darken seen trail
                    unsigned char currentAlpha = cellColor.a;
                    currentAlpha += static_cast<unsigned char>(5); // Increment alpha value

                    // Ensure alpha doesn't exceed maximum value
                    if (currentAlpha > 255) {
                        currentAlpha = 255;
                    }

                    // Update cell color with new alpha value
                    gridMap[currentCell].cellColor = Color{ cellColor.r, cellColor.g, cellColor.b, currentAlpha };
                }
            }
        }
        else { // current cell not in the map
            gridMap[currentCell] = GridCell(CellType::HomeTrail);
            
            Color trailColor = Color{ DARKPURPLE.r, DARKPURPLE.g, DARKPURPLE.b, 10 }; // un seen trail
            gridMap[currentCell].cellColor = trailColor;
        }
        
        if (this->x < 0) {
            this->x = 0;
            this->currentDirection = rand() % 8;
        }
        else if (this->x > screenWidth - gridSize) {
            this->x = screenWidth - gridSize;
            this->currentDirection = rand() % 8;
        }

        if (this->y < 0) {
            this->y = 0;
            this->currentDirection = rand() % 8;
        }
        else if (this->y > screenHeight - gridSize) {
            this->y = screenHeight - gridSize;
            this->currentDirection = rand() % 8;
        }
    }
    
    std::pair<int, int> CurrentCellAntIsOn(int gridSize) {
        return std::pair<int, int>{std::ceil(x / gridSize), std::ceil(y / gridSize)};
    }

    void ChangeState() {
        cout << "State Changed" << endl;
        if (this->antState == AntState::LookingForFood) {
            this->antState = AntState::LookingForHome;
        }
        else {
            this->antState = AntState::LookingForFood;
        }
    }

private:
    int currentDirection;
    float changeDirectionTimer;
    float changeDirectionInterval; // Interval to change direction in seconds
    vector<pair<int, int>> visitedCellsDuringTraversal;

    void ChangeDirection(float deltaTime) {
        // Update timer
        this->changeDirectionTimer += deltaTime;

        // If the timer exceeds the interval, change direction
        if (this->changeDirectionTimer >= this->changeDirectionInterval) {
            this->currentDirection = rand() % 8;
            this->changeDirectionTimer = 0.0f; // Reset the timer
        }
    }
};

void SpawnAnts(std::map<std::pair<int, int>, GridCell>& gridMap, int gridSize, vector<Ant>& allAnts) {
    for (auto const& keyValue : gridMap)
    {

        CellType currCellType = keyValue.second.cellType;

        if (currCellType == CellType::AntHome) { // Ant spawn point
            vector<int> cellCoordinates = ConvertGridCellLocationToCoordinates(gridSize, keyValue.first.first, keyValue.first.second);

            int cellX = cellCoordinates[0];
            int cellY = cellCoordinates[1];

            Ant spawnedAnt = Ant(cellX, cellY, 1, 1, AntState::LookingForFood, AntSpecies::Fire, AntType::Worker);
            allAnts.push_back(spawnedAnt);
        }
    }
}

int main() {
    const int screenWidth = 800;
    const int screenHeight = 600;
    const int gridSize = 12;

    map<std::pair<int, int>, GridCell> gridMap; // col, row of cell (not x,y)
    
    gridMap[{200 / gridSize, 300 / gridSize }] = GridCell(AntHome); // make ant spawn over starting ant's loc
    
    CellType selectedCellType = CellType::Obstacle;


    std::vector<Ant> ants;
    // starting ant
    ants.emplace_back(200, 300, 1, 1, AntState::LookingForFood, AntSpecies::Fire, AntType::Worker);

    // Initialize the window
    InitWindow(screenWidth, screenHeight, "My first RAYLIB program!");
    SetTargetFPS(60);

    float spawnTimer = 0.0f;
    const float spawnInterval = 3.0f; // Spawn ants every 3 seconds

    while (!WindowShouldClose()) { // WindowShouldClose() returns false until the window is closed
        
        float deltaTime = GetFrameTime();
        spawnTimer += deltaTime;

        ChangeDrawingCellType(selectedCellType);

        DrawOverGridCells(gridSize, gridMap, selectedCellType);

        //if (spawnTimer >= spawnInterval) {
            //SpawnAnts(gridMap, gridSize, ants);
            //spawnTimer = 0.0f; // Reset the timer
        //}

        for (auto& ant : ants) {
            //vector<pair<int, int>> pathHome = ant.FindPathHome(gridMap, ant.CurrentCellAntIsOn(gridSize));
            
            //if (!pathHome.empty()) {
                //ant.DrawPathHome(gridSize, pathHome);
            //}
            
            if (ant.antState == LookingForFood) {
                ant.MoveRandomly(screenWidth, screenHeight, gridSize, gridMap); // looking for food
            }
            else if (ant.antState == LookingForHome) {
                ant.TraverseHomePheromone(gridSize, gridMap, screenWidth, screenHeight);
            }
        }

        BeginDrawing();
        ClearBackground(WHITE);

        DrawGridCells(gridSize, screenWidth, screenHeight, gridMap);
        DrawGrid(gridSize, screenWidth, screenHeight);

        for (const auto& ant : ants) {
            DrawCircle(ant.x, ant.y, 5, RED); // Draw the ant as a small red circle
        }


        EndDrawing();
    }

    CloseWindow(); // De-initialize the window
    return 0;
}
