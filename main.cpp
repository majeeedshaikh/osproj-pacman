#include <SFML/Graphics.hpp>
#include <iostream>
#include <pthread.h>
#include <vector>
#include <queue>
#include <unordered_map>
#include <utility>
#include <functional>
#include <algorithm>
#include <map>
#include <unistd.h> // For usleep
#include <semaphore.h> // POSIX semaphores


using namespace std;
// Assuming maze.h contains a global definition of MAZE_HEIGHT, MAZE_WIDTH and the maze array
#include "maze.h"
#include <cmath> // For std::hypot

// Create semaphores for keys and permits
sem_t keySemaphore; // Controls access to the key
sem_t permitSemaphore; // Controls access to the permit


sem_t* firstSemaphore;
sem_t* secondSemaphore;
    
sem_t powerpelletsfull,powerpelletsempty;

bool gameend=0;
bool gameover=0;
int endscore=0;

// Function to calculate Euclidean distance between two points
double distance(int x1, int y1, int x2, int y2) {
    return std::hypot(x2 - x1, y2 - y1);
}
// Custom hash function for std::pair<int, int>
struct pair_hash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        auto hash1 = std::hash<T1>{}(p.first);
        auto hash2 = std::hash<T2>{}(p.second);
        return hash1 ^ hash2;
    }
};

// Custom comparison function for std::pair<int, int> for std::map
struct pair_less {
    bool operator()(const std::pair<int, int>& a, const std::pair<int, int>& b) const {
        return a.first < b.first || (a.first == b.first && a.second < b.second);
    }
};

// Directions for BFS pathfinding
const std::vector<std::pair<int, int>> directions = {
    {0, -1},  // Up
    {0, 1},   // Down
    {-1, 0},  // Left
    {1, 0}    // Right
};

// Player structure to hold position and other attributes
struct Player {
    int x;
    int y;
    int score = 0;
    int lives = 4;
    bool hasPowerPellet = false;
     bool isUnderPowerPelletEffect = false; 
    pthread_mutex_t mutex;
};
 Player player = {1, 1, 0, 3};
// Ghost structure for ghost attributes
struct Ghost {
    int x;
    int y;
    int initialX; // Initial position for respawning
    int initialY;
    bool isOutOfGhostHouse;
    bool isScared=false;
    bool isBlue = false;
    bool isdead=false;
    bool isfast=false;
    bool canbefast=false;
    bool semaphoresout=false;
    pthread_mutex_t mutex;
};
  std::vector<Ghost> ghosts = {{10, 10, 7, 10, false, false}, {10, 10, 9, 11, false, false}, {10, 10, 10, 11, false, false}, {10, 10, 12, 10, false, false}};
  
int deadghostcounter =0;
int deadghostsposition[4][2]={{22,14},{23,14},{24,14},{25,14}};
// Mutexes for resources in the Ghost House
pthread_mutex_t keyMutex;
pthread_mutex_t permitMutex;

// Data structure to remmeber food positions
std::map<std::pair<int, int>, bool, pair_less> foodLocations; // Use pair_less for comparison
pthread_mutex_t foodMutex;

// Data structure to track ghost positions
std::map<std::pair<int, int>, bool, pair_less> ghostPositions; // Holds current ghost positions
pthread_mutex_t ghostPositionsMutex;


// Power pellet locations and mutex
std::map<std::pair<int, int>, bool, pair_less> powerPellets;
pthread_mutex_t powerPelletsMutex;


std::map<std::pair<int, int>, bool, pair_less> speedBoosters;
pthread_mutex_t speedBoostersMutex;
  
pthread_mutex_t uimutex;



// Function to respawn a power pellet at a random location
void respawnPowerPellet() {
    // Choose a random location for the new power pellet
    int newX = rand() % MAZE_WIDTH;
    int newY = rand() % MAZE_HEIGHT;

    // Ensure the chosen location is in an open area of the maze
    while (maze[newY][newX] != 0) {
        newX = rand() % MAZE_WIDTH;
        newY = rand() % MAZE_HEIGHT;
    }

    // Lock the power pellet mutex before updating the power pellets map
    pthread_mutex_lock(&powerPelletsMutex);
    powerPellets[{newX, newY}] = true; // Respawn the power pellet
    pthread_mutex_unlock(&powerPelletsMutex);
}



// Function to respawn a power pellet at a random location
void respawnspeedbooster() {
    // Choose a random location for the new power pellet
    int newX = rand() % MAZE_WIDTH;
    int newY = rand() % MAZE_HEIGHT;

    // Ensure the chosen location is in an open area of the maze
    while (maze[newY][newX] != 0) {
        newX = rand() % MAZE_WIDTH;
        newY = rand() % MAZE_HEIGHT;
    }

    // Lock the power pellet mutex before updating the power pellets map
    pthread_mutex_lock(&speedBoostersMutex);
    speedBoosters[{newX, newY}] = true; // Respawn the power pellet
    pthread_mutex_unlock(&speedBoostersMutex);
}



// BFS pathfinding to find the path
std::vector<std::pair<int, int>> findPath(int startX, int startY, int endX, int endY, bool findShortest) {
    std::queue<std::pair<int, int>> queue;
    std::unordered_map<std::pair<int, int>, std::pair<int, int>, pair_hash> parentMap;
    std::vector<std::pair<int, int>> path;

    queue.push({startX, startY});
    parentMap[{startX, startY}] = {-1, -1};

    while (!queue.empty()) {
        auto current = queue.front();
        queue.pop();

        int currX = current.first;
        int currY = current.second;

        if (currX == endX && currY == endY) {
            while (currX != -1 && currY != -1) {
                path.push_back({currX, currY});
                auto parent = parentMap[{currX, currY}];
                currX = parent.first;
                currY = parent.second;
            }
            std::reverse(path.begin(), path.end());
            break;
        }

        std::vector<std::pair<int, int>> directionList = directions;

        if (!findShortest) {
      std::  cout<<"reversee";
            std::reverse(directionList.begin(), directionList.end());
        }

        for (const auto& dir : directionList) {
            int newX = currX + dir.first;
            int newY = currY + dir.second;

            if (newX >= 0 && newY < MAZE_HEIGHT && maze[newY][newX] == 0 && parentMap.find({newX, newY}) == parentMap.end()) {
                queue.push({newX, newY});
                parentMap[{newX, newY}] = {currX, currY};
            }
        }
    }

    return path;
}


// Thread function to handle power pellet effects
void* handlePowerPelletEffect(void* arg) {
    Player* player = static_cast<Player*>(arg);

    while (!gameend) {
        usleep(10000);  // Check every 10 ms

        if (player->hasPowerPellet && !player->isUnderPowerPelletEffect) {
            player->hasPowerPellet = false;  // Reset power pellet flag
            player->isUnderPowerPelletEffect = true; 
            // Turn all ghosts blue for 3 seconds
            for (int i = 0; i < 4; i++) {
                pthread_mutex_lock(&ghosts[i].mutex);
                ghosts[i].isScared = true;
                ghosts[i].isBlue = true;  // Set ghost state to blue
                pthread_mutex_unlock(&ghosts[i].mutex);
            }

            usleep(8000000);  // Sleep for 3 seconds

            for (int i = 0; i < 4; i++) {
                pthread_mutex_lock(&ghosts[i].mutex);
                ghosts[i].isScared = false;
                ghosts[i].isBlue = false;  // Reset ghost state
                pthread_mutex_unlock(&ghosts[i].mutex);
            }
             player->isUnderPowerPelletEffect = false;
             respawnPowerPellet();
        }
    }

    return nullptr;
}



// Function to handle player movement
void* handlePlayerMovement(void* arg) {
    Player* player = static_cast<Player*>(arg);

    while (!gameend) {
        usleep(100000);  // Sleep for 100 ms
        pthread_mutex_lock(&player->mutex);

        int newX = player->x;
        int newY = player->y;

pthread_mutex_lock(&uimutex);
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::W) && maze[player->y - 1][player->x] == 0) {
                newY--;
            }
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::A) && maze[player->y][player->x - 1] == 0) {
                newX--;
            }
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::S) && maze[player->y + 1][player->x] == 0) {
                newY++;
            }
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::D) and maze[player->y][player->x + 1] == 0) {
                newX++;
            }
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::X) ) {
                gameend=1;
            }
             if (sf::Keyboard::isKeyPressed(sf::Keyboard::O) ) {
                gameover=1;
            }
pthread_mutex_unlock(&uimutex);
        pthread_mutex_lock(&foodMutex);
        if (foodLocations.count({newX, newY}) && foodLocations[{newX, newY}]) {
            foodLocations[{newX, newY}] = false;
            player->score += 10;
        }
        pthread_mutex_unlock(&foodMutex);

///////picking up power pellet
 if (!player->isUnderPowerPelletEffect) {
        // Check for power pellet collisions
        pthread_mutex_lock(&powerPelletsMutex);
        if (powerPellets.count({newX, newY}) && powerPellets[{newX, newY}] ) {
            powerPellets[{newX, newY}] = false;
            player->hasPowerPellet = true;  // Indicate power pellet collected
        }
        pthread_mutex_unlock(&powerPelletsMutex);
}
        player->x = newX;
        player->y = newY;
        pthread_mutex_unlock(&player->mutex);
    }

    return nullptr;
}




void* handleGhostLeavingHouse(void* arg) {
    int ghostNumber = *(static_cast<int*>(arg)); // Retrieve the ghost number passed to the thread
    bool reverseOrder = (ghostNumber == 3); // If the ghost number is 3, reverse the order
      firstSemaphore = reverseOrder ? &permitSemaphore : &keySemaphore;
     secondSemaphore = reverseOrder ? &keySemaphore : &permitSemaphore;
bool haskey=0;
bool haspermit=0;
 cout<<"cominggg inside ghost house"<<endl;
while(!gameend){
    while (  !ghosts[ghostNumber].isOutOfGhostHouse ) {
        std::cout << "Ghost " << ghostNumber << " preparing to leave. Reverse order: " << reverseOrder << std::endl;

        // Acquire the key semaphore
        if (sem_wait(firstSemaphore) == 0) {
            std::cout << "Ghost " << ghostNumber << " acquired key." << std::endl;
            haskey=1;
        } else {
            std::cerr << "Ghost " << ghostNumber << " couldn't acquire key." << std::endl;
            haskey=0;
            usleep(600000); // wait before retrying
            continue;
        }

        // Try to acquire the permit semaphore
        if (sem_trywait(secondSemaphore) == 0) {
            std::cout << "Ghost " << ghostNumber << " acquired permit." << std::endl;
            haspermit=1;
        } else {
            sem_post(firstSemaphore); // Release key if permit is not acquired
            std::cerr << "Ghost " << ghostNumber << " couldn't acquire permit. Releasing key." << std::endl;
            haskey=0;
            haspermit=0;
            usleep(300000); // wait before retrying
            continue;
        }

        // If both key and permit are acquired, allow ghost to leave
        if (haskey && haspermit) {
        
            ghosts[ghostNumber].isOutOfGhostHouse = true;
            std::cout << "Ghost " << ghostNumber << " acquired key and permit. Leaving the ghost house." << std::endl;

            // Simulate ghost leaving the ghost house
            ghosts[ghostNumber].x = ghosts[ghostNumber].initialX;
            ghosts[ghostNumber].y = ghosts[ghostNumber].initialY;
                    
          
        }
    }

  
    }
      return nullptr;
}


// Function to handle ghost movement using the appropriate pathfinding
void* handleGhostMovement(void* arg) {
    Ghost* ghost = static_cast<Ghost*>(arg);

   // while (!ghost->isdead ) {
     while (!gameend ) {
    if(ghost->isfast==1){
        usleep(300000);  // Sleep for 400 ms
}
else{
 usleep(600000);
}


        pthread_mutex_lock(&ghost->mutex);
       
  bool findShortest = true; 

if(!ghost->isBlue){



if(!ghost->isOutOfGhostHouse){
  pthread_mutex_lock(&ghostPositionsMutex);
  ghost->x = ghost->initialX;
              ghost->y = ghost->initialY;
                      pthread_mutex_unlock(&ghostPositionsMutex);
}
else if (ghost->isOutOfGhostHouse){

//cout<<ghost->x<<" "<<ghost->y<<endl;
if((ghost->x<6 ||ghost->x>13 ) || ghost->y>12){
    if(ghost->semaphoresout==false){
    
      sem_post(firstSemaphore);
                  sem_post(secondSemaphore);
                    std::cout << "Ghost  has left the ghost house. Releasing semaphores." << std::endl;
ghost->semaphoresout=true;
    }

}
            auto path = findPath(ghost->x, ghost->y, player.x, player.y, findShortest);

            if (!path.empty() && path.size() > 1) {
                auto nextStep = path[1];
                std::pair<int, int> newPosition(nextStep.first, nextStep.second);

                pthread_mutex_lock(&ghostPositionsMutex);
                if (ghostPositions.find(newPosition) == ghostPositions.end()) { 
                    ghostPositions.erase({ghost->x, ghost->y});
                    ghost->x = newPosition.first;
                    ghost->y = newPosition.second;
                    ghostPositions[newPosition] = true;
                   // cout<<"stuck here"<<endl;
                }
                  
                 pthread_mutex_unlock(&ghostPositionsMutex);
            }

        }
        }
        
      else{
      
      
      
     if (ghost->isOutOfGhostHouse) { 
     
     
     
     
            std::vector<std::pair<int, int>> bestMoves;
            double maxDistance = -1;

            // Check all possible moves to find the one that maximizes distance from the player
            for (const auto& dir : directions) {
                int newX = ghost->x + dir.first;
                int newY = ghost->y + dir.second;

                if (newX >= 0 && newY < MAZE_HEIGHT && maze[newY][newX] == 0) {
                    double dist = distance(newX, newY, player.x, player.y);

                    if (dist > maxDistance) {
                        bestMoves.clear();
                        bestMoves.push_back({newX, newY});
                        maxDistance = dist;
                    } else if (dist == maxDistance) {
                        bestMoves.push_back({newX, newY});
                    }
                }
            }

            // Select one of the best moves randomly to add some variation
            if (!bestMoves.empty()) {
                int choice = std::rand() % bestMoves.size();
                auto chosenMove = bestMoves[choice];

                pthread_mutex_lock(&ghostPositionsMutex);
                if (ghostPositions.find(chosenMove) == ghostPositions.end()) {
                    ghostPositions.erase({ghost->x, ghost->y}); // Release old position
                    ghost->x = chosenMove.first;
                    ghost->y = chosenMove.second;
                    ghostPositions[chosenMove] = true; // Mark the new position
                }
                pthread_mutex_unlock(&ghostPositionsMutex);
            }
        }
      
      
      }

        pthread_mutex_unlock(&ghost->mutex);
    }
    
    return nullptr;
}

// Function to check collision between ghost and speed boosters
void checkGhostSpeedBoosterCollision() {
    for (int i = 0; i < 4; i++) {
        pthread_mutex_lock(&ghosts[i].mutex);
        if (speedBoosters.count({ghosts[i].x, ghosts[i].y}) && speedBoosters[{ghosts[i].x, ghosts[i].y}] && ghosts[i].canbefast==1) {
            // Ghost collided with speed booster
            // Implement logic for increasing ghost speed here
            // For now, let's just remove the speed booster
            ghosts[i].isfast=1;
            speedBoosters[{ghosts[i].x, ghosts[i].y}] = false;
            //respawnspeedbooster();
            cout<<"count"<<endl;
        }
        pthread_mutex_unlock(&ghosts[i].mutex);
    }
}



  
 
sf::Texture speedtexture;
sf::Texture livestexture,lives3;
sf::Texture pacmanTexture;
sf::Texture ghostTexture;
sf::Texture powerPelletTexture;
sf::Texture ghostBlueTexture;
sf::Texture wallTexture;
sf::Texture foodTexture;
sf::Texture scoreTexture;



sf::Sprite speedsprite;
sf::Sprite livestext;
sf::Sprite lives;
sf::Sprite pacmanSprite;
sf::Sprite ghostSprite;
sf::Sprite powerPelletSprite;
sf::Sprite foodSprite;
sf::Sprite wallSprite;
sf::Sprite scoreSprite;
sf::Font font;
   sf::Text scoreText;

void* uithreadfunction(void* arg)
{
pthread_mutex_lock(&uimutex);

     sf::RenderWindow window(sf::VideoMode(1150, 1000), "Pac-Man with Ghosts and Food");
     window.setFramerateLimit(60);

pthread_mutex_unlock(&uimutex);
while(!gameend){


cout<<"inhereee"<<endl;



//pthread_mutex_lock(&uimutex);
sf::Color color(18, 2, 43);

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
        }

        window.clear( color);


// Draw the maze with walls using the wall sprite
for (int y = 0; y < MAZE_HEIGHT; ++y) {
    for (int x = 0; x < MAZE_WIDTH; ++x) {
        if (maze[y][x] == 1) { // Wall
            // Set the sprite's position to the current maze cell
            wallSprite.setPosition(x * 40 + 15 , y * 40 + 15); // Adjust coordinates to match cell size
            // Draw the wall sprite
            window.draw(wallSprite);
        } else { // Path
            // Draw a black rectangle for pathways
            sf::RectangleShape pathRect(sf::Vector2f(40, 40));
            pathRect.setPosition(x * 40, y * 40);
            pathRect.setFillColor(color);
            window.draw(pathRect);
        }
    }
}

        // Draw food with proper synchronization
        
            //std::lock_guard<std::mutex> foodLock(foodMutex);
             pthread_mutex_lock(&foodMutex);
            for (const auto& food: foodLocations) {
                if (food.second) {
                
                

        foodSprite.setPosition(food.first.first  * 40 + 15 ,  food.first.second * 40 + 15); // Adjust coordinates to match cell size
            // Draw the wall sprite
            window.draw(foodSprite);
                    
                    
                    
                    //window.draw(foodShape);
                }
            }
             pthread_mutex_unlock(&foodMutex);
        
// Draw power pellets
        pthread_mutex_lock(&powerPelletsMutex);
        for (const auto& pellet : powerPellets) {
            if (pellet.second) {
                powerPelletSprite.setPosition(pellet.first.first * 40 + 10, pellet.first.second * 40 + 10);  // Positioning
                window.draw(powerPelletSprite);
            }
        }
        pthread_mutex_unlock(&powerPelletsMutex);



        pthread_mutex_lock(&speedBoostersMutex);
        for (const auto& booster : speedBoosters) {
            if (booster.second) {
                speedsprite.setPosition(booster.first.first * 40 + 10, booster.first.second * 40 + 10); // Adjust position
                window.draw(speedsprite);
            }
        }
        pthread_mutex_unlock(&speedBoostersMutex);



        // Lock and unlock mutexes using pthread_mutex_lock and pthread_mutex_unlock
        pthread_mutex_lock(&player.mutex);
        pacmanSprite.setPosition(player.x * 40, player.y * 40);
        pthread_mutex_unlock(&player.mutex);
        window.draw(pacmanSprite);

      
          for (int i = 0; i < 4; i++) {
            pthread_mutex_lock(&ghosts[i].mutex);
            
            if (ghosts[i].isOutOfGhostHouse) {
                if (ghosts[i].isBlue && ghosts[i].isdead==0) {
                
                    ghostSprite = sf::Sprite(ghostBlueTexture);  // Change to blue sprite
                    ghostSprite.setScale(0.02f, 0.02f);
                
                } 
                else {
                
                    ghostSprite = sf::Sprite(ghostTexture);  // Normal sprite
                    ghostSprite.setScale(0.08f, 0.08f);
                    
                }
                
                ghostSprite.setPosition(ghosts[i].x * 40, ghosts[i].y * 40);
                window.draw(ghostSprite);
            }
            
            pthread_mutex_unlock(&ghosts[i].mutex);
        }

        
        
  
      if (!font.loadFromFile("gaming.ttf")) {
          std::cerr << "Error loading font" << std::endl;
          
      }



  
  if(player.lives==2){
  lives3.loadFromFile("2live.png");
  }
   if(player.lives==1){
  lives3.loadFromFile("1live.png");
  }


 
    scoreText.setFont(font);
    scoreText.setCharacterSize(30);
    scoreText.setFillColor(sf::Color::White);
    scoreText.setPosition(900, 120); // Positioning for score
    scoreText.setString( std::to_string(player.score));
    
    endscore=player.score;



// pthread_mutex_lock(&uimutex);
 
 window.draw(livestext);
  window.draw(lives);
  window.draw(scoreSprite);
 // window.draw(bottomdesign);
  window.draw(scoreText);
     
//pthread_mutex_unlock(&uimutex);
         // synchronization phase 1: Check for collisions and handle respawning if necessary
        
                        pthread_mutex_lock(&player.mutex);
                    
                        checkGhostSpeedBoosterCollision();  
                        
  bool collision = false;
  int countercoll=0;

    for (int i = 0; i < 4; ++i) {
        pthread_mutex_lock(&ghosts[i].mutex); // Lock the ghost mutex to access its position

            if (ghosts[i].x == player.x && ghosts[i].y == player.y) 
              {
                      collision = true; // Set collision flag if player collides with any ghost
              }
        pthread_mutex_unlock(&ghosts[i].mutex); // Unlock the ghost mutex after accessing its position
    }

                             
  bool blueghostcollision=0;
  int ghostcollided=5;
     for (int i = 0; i < 4; i++) 
     {
                        pthread_mutex_lock(&ghosts[i].mutex);
                    
                  if( ghosts[i].isBlue==1 && ghosts[i].x == player.x && ghosts[i].y == player.y )
                  {
                      ghostcollided=i;
                        blueghostcollision=1;
                  }
     
                        pthread_mutex_unlock(&ghosts[i].mutex);
      }

  if (collision && !blueghostcollision) 
              {
 
              
            player.lives--;

      if (player.lives > 0) 
              {
                    // Reset player and ghost positions with proper synchronization
                    player.x = 1;
                    player.y = 1;

               

                  for (int i = 0; i < 4; i++) 
                  {
                        pthread_mutex_lock(&ghosts[i].mutex);
                        if(!ghosts[i].isdead){
                      //   cout<<"collison done seeting bools"<<endl;
                              ghosts[i].isOutOfGhostHouse=false;
                              ghosts[i].x = ghosts[i].initialX;
                              ghosts[i].y = ghosts[i].initialY;
                              ghosts[i].isfast=0;
                              ghosts[i].semaphoresout=false;
                                }
                        pthread_mutex_unlock(&ghosts[i].mutex);
                    }


                  // Clear the ghost positions map
                  pthread_mutex_lock(&ghostPositionsMutex);
                      ghostPositions.clear(); 
                  pthread_mutex_unlock(&ghostPositionsMutex);
                  

                    // Reallocate all food
                  pthread_mutex_lock(&foodMutex);
                  for (auto& food : foodLocations) 
                    {
                        food.second = true;
                    }
                  pthread_mutex_unlock(&foodMutex);
              
              
                // Reallocate all speed bosters
                  pthread_mutex_lock(&speedBoostersMutex);
                  for ( auto& booster : speedBoosters) 
                  {
                     booster.second=true;
                  }
                  pthread_mutex_unlock(&speedBoostersMutex);
                            
        
                } 
        else {
                      gameend=1;
                      window.close(); // Close the game if all lives are lost
             }
                
                
            }
      
 else if(collision && blueghostcollision)
            {
           
                        pthread_mutex_lock(&ghosts[ghostcollided].mutex);
                                        ghosts[ghostcollided].isOutOfGhostHouse=false;
                                        ghosts[ghostcollided].x = ghosts[ghostcollided].initialX;
                                        ghosts[ghostcollided].y = ghosts[ghostcollided].initialY;
                                        ghosts[ghostcollided].isfast=0;
                                        ghosts[ghostcollided].semaphoresout=false;
                        pthread_mutex_unlock(&ghosts[ghostcollided].mutex);
           
           
            pthread_mutex_lock(&ghostPositionsMutex);
                      ghostPositions.clear(); // Clear the ghost positions map
            pthread_mutex_unlock(&ghostPositionsMutex);
           
           }
            
             pthread_mutex_unlock(&player.mutex);
        
  
    
        window.display();
        //pthread_mutex_unlock(&uimutex);
        if(gameend==1){
         window.close(); 
        }
    }
      cout<<"coming herr"<<endl;


}



return nullptr;
}



  
void* gameenginethread(void* arg){

  pthread_mutex_lock(&uimutex);



  pacmanTexture.loadFromFile("pacman.png");
  ghostTexture.loadFromFile("ghost.png");
  powerPelletTexture.loadFromFile("4.png");
  ghostBlueTexture.loadFromFile("ghostblue.png");
  wallTexture.loadFromFile("wall.png");
  foodTexture.loadFromFile("5.png");
  scoreTexture.loadFromFile("scoretext1.png");
  speedtexture.loadFromFile("8.png");
  livestexture.loadFromFile("livestext.png");
  lives3.loadFromFile("3live.png"); 
   
   
   
  pacmanSprite.setTexture(pacmanTexture);
  ghostSprite.setTexture(ghostTexture);
  powerPelletSprite.setTexture(powerPelletTexture);
  foodSprite.setTexture(foodTexture);
  wallSprite.setTexture(wallTexture);
  speedsprite.setTexture(speedtexture);
  livestext.setTexture(livestexture);
  lives.setTexture(lives3);
  scoreSprite.setTexture(scoreTexture);



  pacmanSprite.setScale(0.035f, 0.035f);
  ghostSprite.setScale(0.08f, 0.08f);
  powerPelletSprite.setScale(0.09f, 0.09f);
  foodSprite.setScale(0.04f, 0.04f);
  wallSprite.setScale(0.095f, 0.095f);
  speedsprite.setScale(0.06f,0.06f);

  livestext.setScale(0.11f, 0.11f); // Adjust the scale to fit the maze cell
  livestext.setPosition(850, 300); 

  lives.setScale(0.12f, 0.12f); // Adjust the scale to fit the maze cell
  lives.setPosition(830, 380); 
    
  scoreSprite.setScale(0.1f, 0.1f); // Adjust the scale to fit the maze cell
  scoreSprite.setPosition(850, 60); // Positioning for score

  pthread_mutex_unlock(&uimutex);

  while(!gameend){


  }

return NULL;

}

  void* startmenufunction(void* arg){
  pthread_mutex_lock(&uimutex);
  
  // Create the SFML window
    sf::RenderWindow windows(sf::VideoMode(1050, 1000), "SFML Window");

    // Load the start screen texture
    sf::Texture startScreenTexture;
    if (!startScreenTexture.loadFromFile("startscreen.png")) {
        std::cerr << "Error loading start screen texture." << std::endl;
       
    }

    // Create the sprite for the start screen
    sf::Sprite startScreenSprite(startScreenTexture);
startScreenSprite.setScale(0.9f,0.9f);
    while (windows.isOpen()) {
        // Handle events
        sf::Event events;
        while (windows.pollEvent(events)) {
            if (events.type == sf::Event::Closed) {
                windows.close();
            }
            if (events.type == sf::Event::KeyPressed) {
                if (events.key.code == sf::Keyboard::P) {
                    windows.close();
                }
            }
        }

        // Clear the window
        windows.clear();

        // Display the start screen
        windows.draw(startScreenSprite);

        // Display everything
        windows.display();
    }



    pthread_mutex_unlock(&uimutex);
  
  
  
  
  
  
  
  
  
  
    return nullptr;
  
  }
  
  
  void* endmenufunction(void* arg){
  
    
   pthread_mutex_lock(&uimutex);


    sf::RenderWindow windowg(sf::VideoMode(1050, 1000), "SFML Wiindow");

    // Load the start screen texture
    sf::Texture gameovertexture;
    if (!gameovertexture.loadFromFile("gameoverscreen.png")) {
        std::cerr << "Error loading start screen texture." << std::endl;
        
    }

    // Create the sprite for the start screen
    sf::Sprite gameoversprite(gameovertexture);
gameoversprite.setScale(0.9f,0.9f);
  // Positioning for score
    scoreText.setString( std::to_string(endscore));
    while (windowg.isOpen()) {
        // Handle events
        sf::Event eventg;
        while (windowg.pollEvent(eventg)) {
            if (eventg.type == sf::Event::Closed) {
                windowg.close();
            }
            if (eventg.type == sf::Event::KeyPressed) {
                if (eventg.key.code == sf::Keyboard::O) {
                gameover=1;
                    windowg.close();
                }
            }
        }
 scoreText.setPosition(500, 536);
        // Clear the window
        windowg.clear();

        // Display the start screen
        windowg.draw(gameoversprite);
windowg.draw(scoreText);
        // Display everything
        windowg.display();
    }



     pthread_mutex_unlock(&uimutex);
  
  
  
  
  return nullptr;
  }
  
  
  
  
  
  
int main() {

 // GHOST LEAVING HOUSE SEMAPHORES
    if (sem_init(&keySemaphore, 0, 2) != 0 || sem_init(&permitSemaphore, 0, 2) != 0) {
        std::cerr << "Error initializing semaphores." << std::endl;
        return 1;
    }
        
   // MUTEXS INTIALIZATION
   
    pthread_mutex_init(&player.mutex, NULL);
    pthread_mutex_init(&speedBoostersMutex, NULL);
    pthread_mutex_init(&powerPelletsMutex, NULL);
    pthread_mutex_init(&foodMutex, NULL);
    pthread_mutex_init(&uimutex, NULL);
    

    ghosts[2].canbefast=1;
    ghosts[3].canbefast=1;


 // Place PowerPellets in open areas of the maze

    powerPellets[{18, 1}] = true;
    powerPellets[{2, 20}] = true;
    powerPellets[{18, 20}] = true;
    powerPellets[{10, 10}] = true;

 // Place SpeedBoosters in open areas of the maze
    speedBoosters[{5, 10}] = true;
    speedBoosters[{15, 15}] = true;
    
   
 // Place food in open areas of the maze 
   for (int y = 1; y < MAZE_HEIGHT - 1; ++y) {
        for (int x =1; x< MAZE_WIDTH - 1; ++x) {
        
            if (maze[y][x] == 0) {
                foodLocations[{x, y}] = true; // Place food in open spaces
            }
            
        }
    }

  
    pthread_t playerThread,         ghostThreads[8],powerPelletEffectThread,uithread,gameengine,startmenuthread,endmenuthread;


    pthread_create(&startmenuthread, NULL, startmenufunction, NULL);

    pthread_create(&playerThread, NULL, handlePlayerMovement, &player);
    pthread_create(&powerPelletEffectThread, NULL, handlePowerPelletEffect, &player);
    
    pthread_create(&gameengine,NULL,gameenginethread, NULL);
    pthread_create(&uithread, NULL, uithreadfunction, NULL);
    
   int ghostNumbers[4] = {0, 1, 2, 3}; 
   
    for (int i = 0; i < 4; i++) {
    
        pthread_mutex_init(&ghosts[i].mutex, NULL);
        pthread_create(&ghostThreads[i*2], NULL, handleGhostLeavingHouse, &ghostNumbers[i]);
        pthread_create(&ghostThreads[i*2+1], NULL, handleGhostMovement, &ghosts[i]);
        
    }
    
    while(!gameend){
     
     
     
    }

   pthread_create(&endmenuthread, NULL, endmenufunction, NULL);
  
   while(!gameover){
     
     
     
    }
 
    // Clean up
    for (int i = 0; i < 4; i++) {
        pthread_mutex_destroy(&ghosts[i].mutex);
    }
    
   pthread_mutex_destroy(&player.mutex);
   pthread_mutex_destroy(&powerPelletsMutex);
   pthread_mutex_destroy(&foodMutex);
   pthread_mutex_destroy(&uimutex);
    return 0;
    
}
