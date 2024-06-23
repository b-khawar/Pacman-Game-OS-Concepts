#include <SFML/Graphics.hpp>
#include <X11/Xlib.h>
#include <iostream>
#include <cmath> // For math functions
#include <pthread.h>
#include <mutex>
#include <unistd.h>
#include <semaphore.h> // For semaphores
using namespace std;
using namespace sf;
#define N 6 /* Number of threads */
#define RIGHT(i) (((i) + 1) % N)
#define LEFT(i) (((i) == 0) ? (N - 1) : (i) - 1)

typedef enum
{
    THINKING,
    HUNGRY,
    EATING
} thread_state;

int threadID[6] = {1, 2, 3, 4, 5, 6};
thread_state state[N];
sem_t mutexGlobal;
sem_t s[N]; /* One per thread, all initialized to 0 */
pthread_mutex_t mutexCurrDir = PTHREAD_MUTEX_INITIALIZER;


void get_forks(int i)
{
    int l = (i + N - 1) % N;
    int r = (i + 1) % N;
    state[i] = HUNGRY;
    while (state[i] == HUNGRY)
    {
        sem_wait(&mutexGlobal);
        if (state[i] == HUNGRY &&
            state[r] != EATING &&
            state[l] != EATING)
        {
            state[i] = EATING;
            sem_post(&s[i]);
        }
        sem_post(&mutexGlobal);
        sem_wait(&s[i]);
    }
}

void put_forks(int i)
{
    int l = (i + N - 1) % N;
    int r = (i + 1) % N;
    sem_wait(&mutexGlobal);
    state[i] = THINKING;

    if (state[l] == HUNGRY)
        sem_post(&s[l]);

    if (state[r] == HUNGRY)
        sem_post(&s[r]);

    sem_post(&mutexGlobal);
}

const int WIDTH = 800;
const int HEIGHT = 600;
const int CELL_SIZE = 40;          // Size of each cell in pixels
const float PACMAN_SPEED = 150.0f; // Speed of Pacman (pixels per second)
const float GHOST_SPEED = 10.0f;

// Define the maze layout
const int MAZE_ROWS = 15;
const int MAZE_COLS = 20;
std::string maze[MAZE_ROWS] = {
    "####################",
    "#..o...............#",
    "#.###.##.####.####.#",
    "#.#...#..#..#....#.#",
    "#.#...####..#..###.#",
    "#.#......o.....#...#",
    "#.###.#.....#..#...#",
    "#.....#.....#..#...#",
    "#####.#######..#####",
    "#...............o..#",
    "#.###.#.###.#.###..#",
    "#.#...#.....#...#..#",
    "#.#.###.#####.###..#",
    "#.#...o............#",
    "####################"};

// Define the duration of the power pellet effect (in seconds)
const float POWER_PELLET_DURATION = 7.0f;
int direction;                                 // 3,6,9,12
static sf::Vector2i currentDirection = {0, 0}; // Track current movement direction

// Define the power pellet score increment
const int POWER_PELLET_SCORE_INCREMENT = 10;
// sf::Clock powerUpTimer;

// Define Pacman state
enum PacmanState
{
    NORMAL,
    POWERED_UP
};

int score = 0;
int lives = 5;

struct Pacman
{
    sf::Texture pacmanTexture;
    sf::Sprite pacmanSprite;
    int positionX;
    int positionY;
    int directionX;
    int directionY;
    PacmanState state;
    sf::Clock powerUpTimer; // Timer for power-up duration
};

struct Ghost
{
    sf::Texture ghostTexture;
    sf::Sprite ghostSprite;
    sf::Texture scaredTexture;

    // sf::Sprite ghostSprite;
    int positionX;
    int positionY;
    int directionX;
    int directionY;
    bool isScared;
};

// Define a struct to hold the arguments
struct ThreadArgs
{
    Pacman &pacman;
    sf::Clock &moveClock;
    Ghost &ghost;
    Ghost &ghost2;
    Ghost &ghost3;
    Ghost &ghost4;
    sf::RenderWindow &window;
};

struct MazeArgs
{
    sf::RenderWindow &window;
};

struct PowerPill {
    sf::Sprite sprite;
    bool active; // Indicates whether the power pill is active or already consumed
};

PowerPill powerPills[2];


// Function declarations
void handleInput(Pacman &pacman, sf::Clock &moveClock, Ghost &ghost, Ghost &ghost2, Ghost &ghost3, Ghost &ghost4);
void moveGhost(Ghost &ghost, const Pacman &pacman);
void drawMaze(sf::RenderWindow &window);
void *handleInputThread(void *args);
void *moveGhost1Thread(void *args);
void *moveGhost2Thread(void *args);
void *moveGhost3Thread(void *args);
void *moveGhost4Thread(void *args);
void *getInput(void *args);

void *drawMazeThread(void *args);

// Define semaphores for synchronization
sem_t inputSemaphore;
sem_t ghostSemaphore;
sem_t drawSemaphore;

bool isPowerPellet(int x, int y)
{
    return maze[y][x] == 'o';
}

bool isWall(int x, int y)
{
    return maze[y][x] == '#';
}

bool isValidMove(int x, int y)
{
    return x >= 0 && x < MAZE_COLS && y >= 0 && y < MAZE_ROWS && !isWall(x, y);
}

bool isDot(int x, int y)
{
    return maze[y][x] == '.';
}

bool checkCollision(const Sprite &sprite1, const Sprite &sprite2)
{
    return sprite1.getGlobalBounds().intersects(sprite2.getGlobalBounds());
}


void moveGhost(Ghost &ghost, const Pacman &pacman)
{
    
         // current direction
    ghost.positionX += ghost.directionX;
    ghost.positionY += ghost.directionY;

    if (!isValidMove(ghost.positionX, ghost.positionY))
    {   
        // direction changed
        ghost.positionX -= ghost.directionX;
        ghost.positionY -= ghost.directionY;

        int direction = rand() % 4; // 0: up, 1: down, 2: left, 3: right
        switch (direction)
        {
        case 0:
            ghost.directionX = 0;
            ghost.directionY = -1; // Up
            break;
        case 1:
            ghost.directionX = 0;
            ghost.directionY = 1; // Down
            break;
        case 2:
            ghost.directionX = -1;
            ghost.directionY = 0; // Left
            break;
        case 3:
            ghost.directionX = 1;
            ghost.directionY = 0; // Right
            break;
        }
    }

    // updateeeee
    ghost.ghostSprite.setPosition(ghost.positionX * CELL_SIZE, ghost.positionY * CELL_SIZE);
   
   
}

bool checkMaze()
{
    int count = 0;

    for (int i = 0; i < 15; ++i)
    {
        for (int j = 0; j < 20; ++j)
        {
            if (maze[i][j] == '.')
                count++;
        }
    }
    return count;
}

void handleInput(Pacman &pacman, sf::Clock &moveClock, Ghost &ghost, Ghost &ghost2, Ghost &ghost3, Ghost &ghost4)
{
    static float moveDelay = 0.2f; // Adjust this value to change the movement speed

    // handlePowerPellet(pacman);
    //  Move Pacman based on current direction if a movement key is pressed
    if (currentDirection != sf::Vector2i{0, 0})
    {
        if (moveClock.getElapsedTime().asSeconds() >= moveDelay)
        {
            int nextX = pacman.positionX + currentDirection.x;
            int nextY = pacman.positionY + currentDirection.y;

            // Check if the next position is valid (not a wall)
            if (isValidMove(nextX, nextY))
            {
                pacman.positionX = nextX;
                pacman.positionY = nextY;
                // pacman.pacmanSprite.setPosition(pacman.positionX * CELL_SIZE, pacman.positionY * CELL_SIZE);
            }

            moveClock.restart();
        }
    }

    if (maze[pacman.positionY][pacman.positionX] == '.')
    {
        maze[pacman.positionY][pacman.positionX] = ' ';
        score++;
    }

    // Check if Pacman is on a power pellet
    if (isPowerPellet(pacman.positionX, pacman.positionY) && pacman.state != POWERED_UP)
    {
        pacman.state = POWERED_UP;                      // Set Pacman's state to powered up
        pacman.powerUpTimer.restart();                  // Restart the power-up timer
        maze[pacman.positionY][pacman.positionX] = ' '; // Remove the power pellet
        score += 10;                                    // Increment the score
    }

    if (checkCollision(pacman.pacmanSprite, ghost.ghostSprite))
    {
        if (pacman.state == POWERED_UP)
        {
            // Remove the ghost
            ghost.positionX = 8;
            ghost.positionY = 7;
            // ghost.ghostSprite.setPosition(CELL_SIZE * ghost.positionX, CELL_SIZE * ghost.positionY);
            score += 50; // Increment score for eating a ghost
        }
        else
        {
            // Subtract a life and reset Pacman's position
            lives--;
            pacman.positionX = 10;
            pacman.positionY = 13;
            // pacman.pacmanSprite.setPosition(pacman.positionX * CELL_SIZE, pacman.positionY * CELL_SIZE);
        }
    }
    if (checkCollision(pacman.pacmanSprite, ghost2.ghostSprite))
    {
        if (pacman.state == POWERED_UP)
        {
            // Remove the ghost
            ghost2.positionX = 9;
            ghost2.positionY = 7;
            // ghost2.ghostSprite.setPosition(CELL_SIZE * 9, CELL_SIZE * 7);
            score += 50; // Increment score for eating a ghost
        }
        else
        {
            // Subtract a life and reset Pacman's position
            lives--;
            pacman.positionX = 10;
            pacman.positionY = 13;
            // pacman.pacmanSprite.setPosition(pacman.positionX * CELL_SIZE, pacman.positionY * CELL_SIZE);
        }
    }
    if (checkCollision(pacman.pacmanSprite, ghost3.ghostSprite))
    {
        if (pacman.state == POWERED_UP)
        {
            // Remove the ghost
            ghost3.positionX = 10;
            ghost3.positionY = 7;
            // ghost3.ghostSprite.setPosition(CELL_SIZE * 10, CELL_SIZE * 7);
            score += 50; // Increment score for eating a ghost
        }
        else
        {
            // Subtract a life and reset Pacman's position
            lives--;
            pacman.positionX = 10;
            pacman.positionY = 13;
            // pacman.pacmanSprite.setPosition(pacman.positionX * CELL_SIZE, pacman.positionY * CELL_SIZE);
        }
    }

    if (checkCollision(pacman.pacmanSprite, ghost4.ghostSprite))
    {
        if (pacman.state == POWERED_UP)
        {
            // Remove the ghost
            ghost4.positionX = 11;
            ghost4.positionY = 6;
            // ghost3.ghostSprite.setPosition(CELL_SIZE * 10, CELL_SIZE * 7);
            score += 50; // Increment score for eating a ghost
        }
        else
        {
            // Subtract a life and reset Pacman's position
            lives--;
            pacman.positionX = 10;
            pacman.positionY = 13;
            // pacman.pacmanSprite.setPosition(pacman.positionX * CELL_SIZE, pacman.positionY * CELL_SIZE);
        }
    }

    if (checkMaze() == 0)
    {
        for (int i = 0; i < 15; ++i)
        {
            for (int j = 0; j < 20; ++j)
            {
                if (maze[i][j] == ' ')
                {
                    maze[i][j] = '.';
                }
            }
        }
        //----------------------------------------------------------------------------
        maze[1][3] = 'o';
        maze[5][9] = 'o';
        maze[9][16] = 'o';
        maze[13][6] = 'o';
    }
}

void drawMaze(sf::RenderWindow &window)
{
    sf::RectangleShape wall(sf::Vector2f(CELL_SIZE, CELL_SIZE));
    wall.setFillColor(sf::Color::Blue);

    sf::Texture foodTexture;
    foodTexture.loadFromFile("food.png");
    sf::Sprite foodSprite(foodTexture);

    sf::Texture powerpelletTexture;
    powerpelletTexture.loadFromFile("inky(2).png");
    sf::Sprite powerpelletSprite(powerpelletTexture);

    for (int i = 0; i < MAZE_ROWS; ++i)
    {
        for (int j = 0; j < MAZE_COLS; ++j)
        {
            if (maze[i][j] == '#')
            {
                wall.setPosition(j * CELL_SIZE, i * CELL_SIZE);
                window.draw(wall);
            }
            else if (maze[i][j] == '.')
            {
                foodSprite.setPosition(j * CELL_SIZE + 20, i * CELL_SIZE + 20);
                window.draw(foodSprite);
            }
            else if (maze[i][j] == 'o')
            {
                powerpelletSprite.setPosition(j * CELL_SIZE + 20, i * CELL_SIZE + 20);
                window.draw(powerpelletSprite);
            }
        }
    }
}

int main()
{
    XInitThreads();
    sem_init(&mutexGlobal, 1, 1);
    for (int i = 0; i < N; ++i)
    {
        sem_init(&s[i], 1, 0); // Initialize semaphores to 0
    }

    sf::RenderWindow window(sf::VideoMode(WIDTH + 300, HEIGHT), "Pacman");

    // Initialize semaphores
    sem_init(&inputSemaphore, 0, 1); // Initialize input semaphore to 1
    sem_init(&ghostSemaphore, 0, 0); // Initialize ghost semaphore to 0

    sf::Font font;
    font.loadFromFile("TIMESR.ttf"); // load font
    

    Pacman pacman;
    pacman.pacmanTexture.loadFromFile("pacman_right.png");
    pacman.pacmanSprite.setTexture(pacman.pacmanTexture);
    pacman.pacmanSprite.setPosition(CELL_SIZE * 10, CELL_SIZE * 13);
    pacman.directionX = 1; // Initial direction
    pacman.directionY = 0;
    pacman.positionX = 10; // Initial position
    pacman.positionY = 13;
    pacman.state = NORMAL;

    Ghost ghost;
    ghost.ghostTexture.loadFromFile("ghost.png");
    ghost.ghostSprite.setTexture(ghost.ghostTexture);
    ghost.ghostSprite.setPosition(CELL_SIZE * 1, CELL_SIZE * 7);
    ghost.positionX = 8;
    ghost.positionY = 7;
    ghost.directionX = -1;
    ghost.directionY = 0;
    ghost.scaredTexture.loadFromFile("ghost_scared.png");
    

    Ghost ghost2;
    ghost2.ghostTexture.loadFromFile("ghost2.png");
    ghost2.ghostSprite.setTexture(ghost2.ghostTexture);
    ghost2.ghostSprite.setPosition(CELL_SIZE * 9, CELL_SIZE * 7);
    ghost2.positionX = 9;
    ghost2.positionY = 7;
    ghost2.directionX = -1;
    ghost2.directionY = 0;
    ghost2.scaredTexture.loadFromFile("ghost_scared.png");
    

    Ghost ghost3;
    ghost3.ghostTexture.loadFromFile("ghost3.png");
    ghost3.ghostSprite.setTexture(ghost3.ghostTexture);
    ghost3.ghostSprite.setPosition(CELL_SIZE * 10, CELL_SIZE * 7);
    ghost3.positionX = 10;
    ghost3.positionY = 7;
    ghost3.directionX = 1;
    ghost3.directionY = 0;
    ghost3.scaredTexture.loadFromFile("ghost_scared.png");
    

    Ghost ghost4;
    ghost4.ghostTexture.loadFromFile("cute_ghost.png");
    ghost4.ghostSprite.setTexture(ghost4.ghostTexture);
    ghost4.ghostSprite.setPosition(CELL_SIZE * 11, CELL_SIZE * 6);
    ghost4.positionX = 11;
    ghost4.positionY = 6;
    ghost4.directionX = 1;
    ghost4.directionY = 0;
    ghost4.scaredTexture.loadFromFile("ghost_scared.png");

   

    // Load power pellet texture
    sf::Texture powerPelletTexture;
    powerPelletTexture.loadFromFile("inky(2).png");

    // Create a vector to store power pellets
    std::vector<sf::Sprite> powerPellets;

    sf::Texture gameOverTexture;
    if (!gameOverTexture.loadFromFile("gameover.png"))
    {
        cerr << "Failed to load game over texture!" << endl;
        return 1;
    }
    // Create a sprite for the "Game Over" image
    sf::Sprite gameOverSprite(gameOverTexture);

    // Set the position of the "Game Over" sprite
    gameOverSprite.setPosition(330, 200); // Adjust the position as needed

    sf::Clock moveClock;
    // Seed the random number generator
    srand(time(nullptr));
    sf::Clock moveClock3; // for game over

    // Declare pthread_t variables to hold thread IDs
    pthread_t inputThread, ghost1Thread, getInputThread, ghost2Thread, ghost3Thread, ghost4Thread;

    ThreadArgs args = {pacman, moveClock, ghost, ghost2, ghost3, ghost4, window};
    MazeArgs mazeargs = {window};

    pthread_create(&getInputThread, nullptr, getInput, &args);

    pthread_create(&inputThread, nullptr, handleInputThread, &args);
    pthread_create(&ghost1Thread, nullptr, moveGhost1Thread, &args);
    pthread_create(&ghost2Thread, nullptr, moveGhost2Thread, &args);
    pthread_create(&ghost3Thread, nullptr, moveGhost3Thread, &args);
    pthread_create(&ghost4Thread, nullptr, moveGhost4Thread, &args);

    while (window.isOpen())
    {
        // cout << "A\n";
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        if (lives > 0)
        {
            window.clear(sf::Color::Black);

            if (pacman.state == POWERED_UP)
            {
                if (pacman.powerUpTimer.getElapsedTime().asSeconds() >= POWER_PELLET_DURATION)
                    // If so, return Pacman to normal state
                    pacman.state = NORMAL;

                // Set the texture of the ghost sprite to its scared state texture
                ghost.ghostSprite.setTexture(ghost.scaredTexture);
                ghost2.ghostSprite.setTexture(ghost2.scaredTexture);
                ghost3.ghostSprite.setTexture(ghost3.scaredTexture);
                ghost4.ghostSprite.setTexture(ghost4.scaredTexture);
            }
            else
            {
                // Set the texture of the ghost sprite to its normal state texture
                ghost.ghostSprite.setTexture(ghost.ghostTexture);
                ghost2.ghostSprite.setTexture(ghost2.ghostTexture);
                ghost3.ghostSprite.setTexture(ghost3.ghostTexture);
                ghost4.ghostSprite.setTexture(ghost4.ghostTexture);
            }

            // Draw the maze
            drawMaze(window);
            // pthread_create(&drawThread, nullptr, drawMazeThread, &mazeargs);

            // Draw power pellets
            for (const auto &powerPellet : powerPellets)
            {
                window.draw(powerPellet);
            }

            // DISPLAY SCORE
            Text score_text(std::string("Score: " + std::to_string(score)), font, 40);
            score_text.setFillColor(sf::Color::White);
            score_text.setPosition(820, 150);
            window.draw(score_text);

            // DISPLAY LIVES
            Text lives_text(std::string("Lives: " + std::to_string(lives)), font, 40);
            lives_text.setFillColor(sf::Color::White);
            lives_text.setPosition(820, 350);
            window.draw(lives_text);

            pacman.pacmanSprite.setPosition(pacman.positionX * CELL_SIZE, pacman.positionY * CELL_SIZE);
            ghost.ghostSprite.setPosition(CELL_SIZE * ghost.positionX, CELL_SIZE * ghost.positionY);
            ghost2.ghostSprite.setPosition(CELL_SIZE * ghost2.positionX, CELL_SIZE * ghost2.positionY);
            ghost3.ghostSprite.setPosition(CELL_SIZE * ghost3.positionX, CELL_SIZE * ghost3.positionY);
            ghost4.ghostSprite.setPosition(CELL_SIZE * ghost4.positionX, CELL_SIZE * ghost4.positionY);

            // Draw Pacman
            window.draw(pacman.pacmanSprite);
            window.draw(ghost.ghostSprite);
            window.draw(ghost2.ghostSprite);
            window.draw(ghost3.ghostSprite);
            window.draw(ghost4.ghostSprite);
        }
        else // Game Over
        {
            window.clear(sf::Color::Black);
            window.draw(gameOverSprite);
            window.display();
            sf::sleep(sf::seconds(5)); // Display game over screen for 5 seconds
            break;                     // Exit the game loop after displaying game over screen
        }
        window.display();
        usleep(100000 / 60);
    }

    cout << "B\n";
    // Destroy semaphores
    sem_destroy(&inputSemaphore);
    sem_destroy(&ghostSemaphore);
    sem_destroy(&drawSemaphore);

    return 0;
}

// Function definitions
void *handleInputThread(void *args_ptr)
{
    ThreadArgs *args = reinterpret_cast<ThreadArgs *>(args_ptr);
    while (args->window.isOpen())
    {

        get_forks(threadID[0] - 1);
        pthread_mutex_lock(&mutexCurrDir);
        handleInput(args->pacman, args->moveClock, args->ghost, args->ghost2, args->ghost3, args->ghost4);
        pthread_mutex_unlock(&mutexCurrDir);
        put_forks(threadID[0] - 1);
        usleep(100000 / 60);
    }
    pthread_exit(nullptr);
    return nullptr;
}

void *moveGhost1Thread(void *args_ptr)
{
    ThreadArgs *args = reinterpret_cast<ThreadArgs *>(args_ptr);
    sf::Clock moveClock2;
    float ghostStartDelay = 3.0f; // Delay for ghost movement start
    float elapsed = 0.0f; // Elapsed time

    while (args->window.isOpen())
    {
        // Check if the delay has passed
        if (elapsed >= ghostStartDelay)
        {
            get_forks(threadID[1] - 1);
            if (moveClock2.getElapsedTime().asSeconds() >= 1.0f / GHOST_SPEED)
            {
                moveGhost(args->ghost, args->pacman);
                moveClock2.restart();
            }
            put_forks(threadID[1] - 1);
        }
        else
        {
            // Increment elapsed time
            elapsed += moveClock2.restart().asSeconds();
        }
        
        usleep(100000 / 60);
    }
    pthread_exit(nullptr);
    return nullptr;
}


void *moveGhost2Thread(void *args_ptr)
{
    ThreadArgs *args = reinterpret_cast<ThreadArgs *>(args_ptr);
    sf::Clock moveClock2;
    float ghostStartDelay = 6.0f; // Delay for ghost movement start
    float elapsed = 0.0f; // Elapsed time

    while (args->window.isOpen())
    {
        // Check if the delay has passed
        if (elapsed >= ghostStartDelay)
        {
            get_forks(threadID[3] - 1);
            if (moveClock2.getElapsedTime().asSeconds() >= 1.0f / GHOST_SPEED)
            {
                moveGhost(args->ghost2, args->pacman);
                moveClock2.restart();
            }
            put_forks(threadID[3] - 1);
        }
        else
        {
            // Increment elapsed time
            elapsed += moveClock2.restart().asSeconds();
        }
        
        usleep(100000 / 60);
    }
    pthread_exit(nullptr);
    return nullptr;
}


void *moveGhost3Thread(void *args_ptr)
{
    ThreadArgs *args = reinterpret_cast<ThreadArgs *>(args_ptr);
    sf::Clock moveClock2;
    float ghostStartDelay = 11.0f; // Delay for ghost movement start
    float elapsed = 0.0f; // Elapsed time

    while (args->window.isOpen())
    {
        // Check if the delay has passed
        if (elapsed >= ghostStartDelay)
        {
            get_forks(threadID[4] - 1);
            if (moveClock2.getElapsedTime().asSeconds() >= 1.0f / GHOST_SPEED)
            {
                moveGhost(args->ghost3, args->pacman);
                moveClock2.restart();
            }
            put_forks(threadID[4] - 1);
        }
        else
        {
            // Increment elapsed time
            elapsed += moveClock2.restart().asSeconds();
        }
        
        usleep(100000 / 60);
    }
    pthread_exit(nullptr);
    return nullptr;
}

void *moveGhost4Thread(void *args_ptr)
{
    ThreadArgs *args = reinterpret_cast<ThreadArgs *>(args_ptr);
    sf::Clock moveClock2;
    float ghostStartDelay = 15.0f; // Delay for ghost movement start
    float elapsed = 0.0f; // Elapsed time

    while (args->window.isOpen())
    {
        // Check if the delay has passed
        if (elapsed >= ghostStartDelay)
        {
            get_forks(threadID[5] - 1);
            if (moveClock2.getElapsedTime().asSeconds() >= 0.5f / GHOST_SPEED)
            {
                moveGhost(args->ghost4, args->pacman);
                moveClock2.restart();
            }
            put_forks(threadID[5] - 1);
        }
        else
        {
            // Increment elapsed time
            elapsed += moveClock2.restart().asSeconds();
        }
        
        usleep(100000 / 60);
    }
    pthread_exit(nullptr);
    return nullptr;
}

void *drawMazeThread(void *args_ptr)
{
    // MazeArgs *args = reinterpret_cast<MazeArgs *>(args_ptr);
    ThreadArgs *args = reinterpret_cast<ThreadArgs *>(args_ptr);
    while (args->window.isOpen())
    {

        get_forks(threadID[2] - 1);
        drawMaze(args->window);
        put_forks(threadID[2] - 1);
        usleep(100000 / 60);
    }
    pthread_exit(nullptr);
    return nullptr;
}

void *getInput(void *args_ptr)
{
    ThreadArgs *args = reinterpret_cast<ThreadArgs *>(args_ptr);

    while (args->window.isOpen())
    {

        get_forks(threadID[2] - 1);
        pthread_mutex_lock(&mutexCurrDir);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up))
        {
            currentDirection = {0, -1};
            args->pacman.pacmanTexture.loadFromFile("pacman_up.png");
        }
        else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down))
        {
            currentDirection = {0, 1};
            args->pacman.pacmanTexture.loadFromFile("pacman_down.png");
        }
        else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))
        {
            currentDirection = {-1, 0};
            args->pacman.pacmanTexture.loadFromFile("pacman_left.png");
        }
        else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right))
        {
            currentDirection = {1, 0};
            args->pacman.pacmanTexture.loadFromFile("pacman_right.png");
        }
        pthread_mutex_unlock(&mutexCurrDir);
        put_forks(threadID[2] - 1);
        usleep(100000 / 60);
    }
    pthread_exit(nullptr);
    return nullptr;
}
