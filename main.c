#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAX_ENEMIES 1000
#define MAX_PROJECTILES 1000
#define BLOCK_SIZE 16
#define MAX_WIDTH 1000
#define MAX_HEIGHT 100
#define SCREEN_WIDTH 1200
#define SCREEN_HEIGHT 600
#define MAX_NOME 20
#define MAX_HISTORY_SIZE 180

typedef struct {
    Vector2 position;   // Coordenadas (x, y)
    Vector2 velocity;   // Velocidade (x, y)
    Rectangle rect;     // Retangulo pra colisao
    bool isGrounded;    // Determina se está no chao
    bool facingRight;   // Determina direcao que está encarando
    bool isShooting;    // Determina se está atirando ou nao
    int health;         // Pontos de vida
    int points;         // Pontos para o placar
    char nome[MAX_NOME];
    Vector2 spawnPoint;
} Player;

typedef struct {
    char nome[MAX_NOME];
    int points;
} JogadorLeader;

typedef struct {
    Vector2 position;   // coordenadas (x, y)
    Vector2 velocity;   // velocidade (x, y)
    Rectangle rect;     // Retangulo pra colisao
    Vector2 minPosition; // posicao minima (x, y)
    Vector2 maxPosition; // posicao maxima (x, y)
    int health;         // pontos de vida
    bool active;        // determina se o inimigo está ativo
} Enemy;

typedef struct {
    Rectangle rect;  // Propriedades de posição e tamanho
    Vector2 speed;   // Velocidade do projétil
    bool active;     // Indica se o projétil está ativo
    Color color;
} Projectile;

typedef struct {
    Vector2 position;   // Coordenadas (x, y)
    Rectangle rect;     // Retângulo para colisão
    bool active;        // Se a moeda está ativa ou não
    int points;         // Quantidade de pontos que a moeda dá
} Coin;

typedef struct {
    Vector2 positionHistory[MAX_HISTORY_SIZE]; // Coordenadas (x,y) ; [Array de posições possíveis do jogador]
    int currentIndex; // E o currentIndex indica qual dessas posições no array ele está
} PlayerHistory;

// Le o mapa a partir de um arquivo
void LoadMap(const char* filename, char map[MAX_HEIGHT][MAX_WIDTH], int* rows, int* cols) {
    FILE* file = fopen(filename, "r");  // Le o arquivo
    if (!file) {
        perror("Failed to open file");
        return;
    }

    // Inicia contagem das linhas e colunas em 0
    *rows = 0;
    *cols = 0;

    char line[MAX_WIDTH];  // buffer pra armazenar cada linha e coluna
    while (fgets(line, sizeof(line), file)) { // Le linha por linha
        size_t len = strlen(line);  // Recebe o comprimento da linha atual
        if (line[len - 1] == '\n') line[len - 1] = '\0';  // Remove o caractere de nova linha

        // Copia linha pro array
        strncpy(map[*rows], line, MAX_WIDTH);

        (*rows)++;  // Incrementa contagem da linha
        if (len > *cols) *cols = len - 1;  // Atualiza contagem da coluna se a linha atual é maior do que a anterior
    }

    fclose(file);

    if (*rows <= 10 || *cols <= 200) {
        printf("Mapa menor do que 200x10");
        exit(1);
    }
}

// Aplica calculo da gravidade
void ApplyGravity(Player *player, float gravity, float dt) {
    player->velocity.y += gravity * dt;
}

// Determina ou não se existe um spawnpoint para o jogador, caso sim, aplica as coordenadas encontradas no array x e y como ponto inicial do jogador
bool FindPlayerSpawnPoint(char map[MAX_HEIGHT][MAX_WIDTH], int rows, int cols, Player* player) {
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            if (map[y][x] == 'P') { // letra P no mapa encontrada
                player->spawnPoint = (Vector2){x * BLOCK_SIZE, y * BLOCK_SIZE};
                player->position = player->spawnPoint;
                player->rect.x = player->position.x;
                player->rect.y = player->position.y;


                return true; // Existe spawnpoint
            }
        }
    }
    return false; // Nenhuma letra P foi encontrada, nao existe spawnpoint
}

void UpdateEnemyAnimationState(Enemy *enemy, float *frameTimer, float frameSpeed, unsigned *currentFrame, Rectangle *frameRec, int frameWidth) {
    *frameTimer += GetFrameTime();
    if (*frameTimer >= frameSpeed) {
        *frameTimer = 0.0f;

        *currentFrame = (*currentFrame == 0) ? 1 : 0;
    }

    frameRec->x = frameWidth * (*currentFrame);
    frameRec->width = frameWidth;
}

void RenderBackground(Texture2D background, int rows, int cols) {
    for (int i = 0; i < cols; i++) {
        for (int j = 0; j < rows; j++) {
            DrawTexture(background, i * background.width, j * background.height, BLUE);
        }
    }
}


// Renderiza moedas
void RenderCoins(Coin coins[MAX_WIDTH], int coinCount) {
    for (int i = 0; i < coinCount; i++) {
        if (coins[i].active) {
            DrawRectangleRec(coins[i].rect, YELLOW);  // Draw coin as a rectangle (yellow color)
        }
    }
}

// Renderiza projeteis
void RenderProjectiles(Projectile projectiles[MAX_PROJECTILES]) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].active) {
            DrawRectangleRec(projectiles[i].rect, projectiles[i].color);
        }
    }
}

// Renderiza inimigos
void RenderEnemies(Enemy enemies[MAX_ENEMIES], int enemyCount, float blockSize, Texture2D enemyTexture,
                   Rectangle *enemyFrameRec, float *frameTimer, unsigned *currentFrame) {
    for (int i = 0; i < enemyCount; i++) {
        if (enemies[i].active) {
            UpdateEnemyAnimationState(&enemies[i], frameTimer, 0.5f, currentFrame, enemyFrameRec, 16);

            Rectangle destRect = {enemies[i].position.x, enemies[i].position.y, enemyFrameRec->width, enemyFrameRec->height};

            DrawTexturePro(
                enemyTexture,
                *enemyFrameRec,
                destRect,
                (Vector2){0, 0},
                0.0f,
                WHITE
            );
        }
    }
}

// Renderiza mapa
void RenderMap(char map[MAX_HEIGHT][MAX_WIDTH], int rows, int cols, float blockSize, Texture2D blockTexture, Texture2D obstacleTexture, Texture2D gateTexture) {
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            if (map[y][x] == 'B') {
                Rectangle destRect = {x * blockSize, y * blockSize, blockSize, blockSize};
                DrawTexturePro(blockTexture, (Rectangle){0, 0, blockTexture.width, blockTexture.height}, destRect, (Vector2){0, 0}, 0.0f, WHITE);
            } else if (map[y][x] == 'O') {
                Rectangle destRect = {x * blockSize, y * blockSize, blockSize, blockSize};
                DrawTexturePro(obstacleTexture, (Rectangle){0, 0, obstacleTexture.width, obstacleTexture.height}, destRect, (Vector2){0, 0}, 0.0f, WHITE);
            } else if (map[y][x] == 'G') {
                Rectangle destRect = {x * blockSize, y * blockSize - 16, blockSize * 2, blockSize * 2};
                DrawTexturePro(gateTexture, (Rectangle) {0, 0, gateTexture.width, gateTexture.height}, destRect, (Vector2){0, 0}, 0.0f, WHITE);
            }
        }
    }
}

// Inicializacao do jogador
Player InitializePlayer() {
    Player player = {
        {0, 0},
        {0, 0},
        {0, 0, 32, 32},
        false,
        true,
        false,
        3,
        0,
        " \n",
        0
    };
    return player;
}

// Inicializacao da camera
Camera2D InitializeCamera(Player *player) {
    Camera2D camera = {0};
    camera.target = (Vector2) {
        player->position.x + player->rect.width / 2, player->position.y + player->rect.height / 2
    };
    camera.offset = (Vector2) {
        SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f
    };
    camera.zoom = 2.0f;
    return camera;
}

// Inicializacao das texturas do jogador
void InitializePlayerTextureAndAnimation(Texture2D *infmanTex, Rectangle *frameRec, int *frameWidth, Texture2D *enemyTex, Rectangle *enemyFrameRec, int *enemyFrameWidth) {
    *infmanTex = LoadTexture("player-sheet.png");
    *frameWidth = infmanTex->width / 12;
    *frameRec = (Rectangle){0.0f, 0.0f, (float)(*frameWidth), (float)infmanTex->height};

    *enemyTex = LoadTexture("enemies.png");
    *enemyFrameWidth = enemyTex->width / 2;
    *enemyFrameRec = (Rectangle){0.0f, 0.0f, (float)(*enemyFrameWidth), (float)enemyTex->height};
}

// Inicializacao dos projeteis
void InitializeProjectiles(Projectile projectiles[MAX_PROJECTILES]) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        projectiles[i].active = false;
    }
}

// Encontra instancias da letra "M" no arquivo e criar inimigos pra cada uma delas
int InitializeEnemies(char map[MAX_HEIGHT][MAX_WIDTH], int rows, int cols, Enemy enemies[MAX_WIDTH], float blockSize, float enemySpeedX, float enemySpeedY, float offset) {
    int enemyCount = 0;

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            if (map[y][x] == 'M') {
                enemies[enemyCount].position = (Vector2){x * blockSize, y * blockSize};
                enemies[enemyCount].velocity = (Vector2){enemySpeedX, enemySpeedY}; // Velocidade do inimigo
                //LINHA PARA MUDAR ABAIXO
                enemies[enemyCount].rect = (Rectangle){enemies[enemyCount].position.x, enemies[enemyCount].position.y, blockSize, blockSize};
                enemies[enemyCount].minPosition = enemies[enemyCount].position; // Posicao minimia é o spawnpoint
                enemies[enemyCount].maxPosition = (Vector2){enemies[enemyCount].position.x + offset, enemies[enemyCount].position.y}; // Posicao maxima
                enemies[enemyCount].health = 1; // Vida que começa
                enemies[enemyCount].active = true; // Inimigo é ativado
                enemyCount++;
            }
        }
    }

    return enemyCount;
}

// Inicializa as moedas no mapa
int InitializeCoins(char map[MAX_HEIGHT][MAX_WIDTH], int rows, int cols, Coin coins[MAX_WIDTH], float blockSize) {
    int coinCount = 0;

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            if (map[y][x] == 'C') { // Moeda no mapa
                coins[coinCount].position = (Vector2)
                {
                    x * blockSize, y * blockSize
                };
                coins[coinCount].rect = (Rectangle)
                {
                    coins[coinCount].position.x, coins[coinCount].position.y, blockSize, blockSize
                };
                coins[coinCount].active = true; // Marca a moeda como ativa
                coins[coinCount].points = 10; // A moeda dá 10
                coinCount++;
            }
        }
    }

    return coinCount;
}

void HandleRespawn(Player *player, float screenHeight) {
    if (player->position.y > screenHeight) {
        player->position = player->spawnPoint;
        player->velocity = (Vector2){0, 0};
        player->health -= 1;
    }
}

// Aplica movimento para o jogador conforme a tecla pressionada
void CheckPressedKey(Player *player, float moveSpeed, float jumpForce) {
    player->velocity.x = 0;

    if (IsKeyDown(KEY_RIGHT)) {
        player->velocity.x = moveSpeed;
        player->facingRight = true;
    }
    if (IsKeyDown(KEY_LEFT)) {
        player->velocity.x = -moveSpeed;
        player->facingRight = false;
    }
    if (IsKeyPressed(KEY_SPACE) && player->isGrounded) {
        player->velocity.y = jumpForce;
        player->isGrounded = false;
    }
}

// Cria projetil com coordenadas baseadas na posição atual do jogador e aplica estado do jogador estar atirando durante 0.5 segundos
void CreateProjectile(Player *player, Projectile projectiles[MAX_PROJECTILES], float projectileWidth, float projectileHeight, float projectileSpeed, float dt) {
    static float shootTimer = 0.0f;
    float animationDuration = 0.5;

    if (IsKeyPressed(KEY_Z)) {
        player->isShooting = true;

        for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (!projectiles[i].active) {

                projectiles[i].rect = (Rectangle) {
                    player->position.x + (player->facingRight ? player->rect.width : -projectileWidth),
                    player->position.y + player->rect.height / 2 - projectileHeight / 2,
                    projectileWidth,
                    projectileHeight
                };
                projectiles[i].speed = (Vector2) {
                    player->facingRight ? projectileSpeed : -projectileSpeed, 0
                };
                projectiles[i].color = YELLOW;
                projectiles[i].active = true;
                break;
            }
        }
    }

    if (IsKeyPressed(KEY_X)) {
        player->isShooting = true;

        for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (!projectiles[i].active) {
                projectiles[i].rect = (Rectangle){
                    player->position.x + (player->facingRight ? player->rect.width : -projectileHeight),
                    player->position.y + player->rect.height / 2 - projectileWidth / 2,
                    projectileHeight,
                    projectileWidth
                };
                if(player->isGrounded) {
                    projectiles[i].speed = (Vector2){0,-400};
                } else {
                    projectiles[i].speed = (Vector2){0,400};
                }
                projectiles[i].active = true;
                projectiles[i].color = BLUE; // Blue projectile
                break;
            }
        }
    }

    if (player->isShooting) {
        shootTimer += dt;
        if (shootTimer >= animationDuration) {
            player->isShooting = false;
            shootTimer = 0.0f;
        }
    }
}

// Move camera de acordo com posição do jogador
void MoveCamera(Camera2D *camera, Player *player) {
    camera->target = (Vector2)
    {
        player->position.x + player->rect.width / 2, SCREEN_HEIGHT / 2 - 110
    };
}

// Move jogador com base na velocidade multiplicada pelo frame atual
void MovePlayer(Player *player, float moveSpeed, float jumpForce, float dt) {
    CheckPressedKey(player, moveSpeed, jumpForce);
    player->position.x += player->velocity.x * dt;
    player->position.y += player->velocity.y * dt;
    player->rect.x = player->position.x;
    player->rect.y = player->position.y;
}

// Move os inimigos com base na velocidade multiplicada pelo frame atual
void MoveEnemies(Enemy* enemies, int enemyCount, float dt) {
    for (int i = 0; i < enemyCount; i++) {
        // Faz o inimigo ir e voltar
        if (enemies[i].position.x <= enemies[i].minPosition.x || enemies[i].position.x >= enemies[i].maxPosition.x) {
            enemies[i].velocity.x = -enemies[i].velocity.x; // Inverte direção
        }
        enemies[i].position.x += enemies[i].velocity.x * dt;
        enemies[i].rect.x = enemies[i].position.x;
        enemies[i].rect.y = enemies[i].position.y;
    }
}

// Desativa projeteis quando batem em um bloco
void CheckProjectileBlockCollision(Projectile *projectile, Rectangle block) {
    if (CheckCollisionRecs(projectile->rect, block)) {
        projectile->active = false;
    }
}

// Move projeteis quanndo disparados
void MoveProjectiles(Projectile projectiles[MAX_PROJECTILES], float dt, Player* player, int screenWidth, char map[MAX_HEIGHT][MAX_WIDTH], int rows, int cols, float blockSize) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].active) {
            // Movimento do projetil
            projectiles[i].rect.x += projectiles[i].speed.x * dt;
            projectiles[i].rect.y += projectiles[i].speed.y * dt;

            // Desativa se vai pra fora da tela
            if (projectiles[i].rect.x < player->position.x - screenWidth ||
                    projectiles[i].rect.x > player->position.x + screenWidth ||
                    projectiles[i].rect.y < player->position.y - screenWidth ||
                    projectiles[i].rect.y > player->position.y + screenWidth)
            {
                projectiles[i].active = false;
            }

            // Verifica colisao com blocos
            for (int y = 0; y < rows; y++) {
                for (int x = 0; x < cols; x++) {
                    if (map[y][x] == 'B') {  // Verifica bloco a bloco ate achar um
                        Rectangle block = {x * blockSize, y * blockSize, blockSize, blockSize};
                        CheckProjectileBlockCollision(&projectiles[i], block);
                    }
                }
            }
        }
    }
}


// Colisao entre jogador e moeda
void CheckPlayerCoinCollision(Player* player, Coin* coins, int* coinCount) {
    for (int i = 0; i < *coinCount; i++) {
        if (coins[i].active && CheckCollisionRecs(player->rect, coins[i].rect)) {
            player->points += coins[i].points;
            coins[i].active = false;
            printf("Points: %d\n", player->points);
        }
    }
}

// Verifica colisão entre o projétil e inimigo
void CheckProjectileEnemyCollision(Projectile* projectiles, int* enemyCount, Enemy* enemies, Player* player) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (projectiles[i].active) {
            for (int j = 0; j < *enemyCount; j++) {
                if (enemies[j].active && CheckCollisionRecs(projectiles[i].rect, enemies[j].rect)) {
                    // Colisão detectada reduz a vida do inimigo
                    enemies[j].health -= 1;  // Diminui a vida
                    player->points += 100;
                    if (enemies[j].health <= 0)
                    {
                        enemies[j].health = 0;
                        enemies[j].active = false; // Desativa o inimigo se a vida chegar a 0
                    }
                    projectiles[i].active = false;  // Desativa projetil após colisão
                    break;
                }
            }
        }
    }
}

// Verifica colisao entre jogador e blocos
bool CheckCollisionWithBlock(Rectangle player, Rectangle block, Vector2* correction) {
    if (CheckCollisionRecs(player, block)) {
        // TODO: Comentar o que fmin e fmax fazem e explicar código
        float overlapX = fmin(player.x + player.width, block.x + block.width) - fmax(player.x, block.x);
        float overlapY = fmin(player.y + player.height, block.y + block.height) - fmax(player.y, block.y);

        // Resolve com base na menor sobreposicao
        if (overlapX < overlapY) {
            correction->x = (player.x < block.x) ? -overlapX : overlapX;
        } else {
            correction->y = (player.y < block.y) ? -overlapY : overlapY;
        }

        return true;
    }

    return false;
}
// Dado um texto para preencher e uma coordenada, desenha um retângulo
Rectangle CreateMenuButton(const char *text, int yOffset) {
    return (Rectangle) {
        SCREEN_WIDTH / 2 - MeasureText(text, 50) / 2,
                     SCREEN_HEIGHT / 2 + yOffset,
                     MeasureText(text, 50),
                     50
    };
}

// Desenha botão com efeito de trocar de cor quando mouse passa por cima
void DrawButton(Rectangle button, const char *text, Vector2 mouse, Color hoverColor, Color defaultColor) {
    Color buttonColor = CheckCollisionPointRec(mouse, button) ? hoverColor : defaultColor;
    DrawRectangleRec(button, buttonColor);
    DrawText(text, button.x + button.width / 2 - MeasureText(text, 20) / 2, button.y + 15, 20, BLACK);
}

// Detecta se mouse clicou em cima do botão
int HandleButtonClick(Rectangle button, Vector2 mouse) {
    return CheckCollisionPointRec(mouse, button) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

// Caso haja colisão entre jogador e o bloco, e bloco seja M, usa a diferença entre as duas posições, a variavel correction, é usada para manter o jogador na sua posição.
void HandleBlockCollision(Player *player, Rectangle block) {
    Vector2 correction = {0, 0};

    if (CheckCollisionWithBlock(player->rect, block, &correction)) {
        if (correction.y != 0) {  // Correção vertical
            player->velocity.y = 0;
            player->position.y += correction.y;

            if (correction.y < 0) {
                player->isGrounded = true;
            }
        } else if (correction.x != 0) {     // Correção horizontal
            player->velocity.x = 0;
            player->position.x += correction.x;
        }

        player->rect.x = player->position.x;
        player->rect.y = player->position.y;
    }
}

int Menu(void) {
    Texture2D initializeTexture = LoadTexture("inf_man.png");

    // Inicialização dos botões
    Rectangle start = CreateMenuButton("Iniciar", 0);
    Rectangle leaderboard = CreateMenuButton("Placar de pontos", 100);
    Rectangle exit = CreateMenuButton("Sair", 200);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        Vector2 mouse = GetMousePosition();

        // Renderiza plano de fundo
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, DARKBLUE);
        DrawTexture(initializeTexture, SCREEN_WIDTH / 2 - initializeTexture.width / 2, SCREEN_HEIGHT / 4, WHITE);

        // Renderiza botões
        DrawButton(start, "Iniciar", mouse, YELLOW, LIGHTGRAY);
        if (HandleButtonClick(start, mouse)) {
            return 1;
        }

        DrawButton(leaderboard, "Placar de pontos", mouse, YELLOW, LIGHTGRAY);
        if (HandleButtonClick(leaderboard, mouse)) {
            return 2;
        }

        DrawButton(exit, "Sair", mouse, YELLOW, LIGHTGRAY);
        if (HandleButtonClick(exit, mouse)) {
            return 3;
        }

        EndDrawing();
    }

    return 0;
}
// Desenha a tela final do jogo, retornando uma telaa de vitória ou derrota
void DesenhaTelaFinal(void) {
    while(!IsKeyPressed(KEY_ENTER)) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RED);
        DrawText("YOU FAILED!",(SCREEN_WIDTH - MeasureText("PARABENS! VOCE GANHOU", 60)) / 2, (SCREEN_HEIGHT - 30) / 2, 60, WHITE);
        DrawText("Aperte enter para voltar ao menu", (SCREEN_WIDTH - MeasureText("PARABENS! VOCE GANHOU", 60)) / 2, (SCREEN_HEIGHT + 60) / 2, 40, WHITE);
        // volta para o menu
        EndDrawing();
    }
}
// Colisao entre jogador e inimigo
void HandlePlayerEnemyCollision(Player* player, Enemy* enemies, int enemyCount, int* currentFrame, float dt) {
    for (int i = 0; i < enemyCount; i++) {
        if (CheckCollisionRecs(player->rect, enemies[i].rect) && enemies[i].active) {
            player->health -= 1;
            *currentFrame = 11;

            player->position = player->spawnPoint;
            player->velocity = (Vector2){0, 0};

            player->rect.x = player->position.x;
            player->rect.y = player->position.y;
        }
    }
}

// Caso haja colisão entre jogador e o bloco, e bloco seja O, empurra o jogador para trás de subtrai 1 de sua vida.
void HandleObstacleCollision(Player *player, Rectangle block) {
    Vector2 correction = {0, 0};
    if (CheckCollisionWithBlock(player->rect, block, &correction)) {
        player->health -= 1;


        // Reset player to position from 3 seconds ago
        player->position = player->spawnPoint;
        player->velocity = (Vector2) {0, 0};

        // Update player's rect position
        player->rect.x = player->position.x;
        player->rect.y = player->position.y;

        printf("Player health: %d\n", player->health);
    }
}

// Compara dois jogadores
int ComparaJogadores(JogadorLeader PlayerA, JogadorLeader PlayerB) {
    // Ordena por pontos de forma decrescente
    if (PlayerA.points != PlayerB.points) {
        return PlayerB.points - PlayerA.points; // Maior pontuação primeiro
    }

    // Se as pontuações forem iguais, ordena por nome em ordem alfabética
    return strcmp(PlayerA.nome, PlayerB.nome);
}

// Ordena os jogadores por pontuação (decrescente)
void OrdenaPlayers(JogadorLeader jogadores[]) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4 - i; j++) {
            if (ComparaJogadores(jogadores[j], jogadores[j + 1]) > 0) {
                JogadorLeader temp = jogadores[j];
                jogadores[j] = jogadores[j + 1];
                jogadores[j + 1] = temp;
            }
        }
    }
}

// Cria o arquivo top_scores.bin com jogadores fictícios
void CriaTop5Jogadores(void) {
    FILE *arq;
    JogadorLeader Players[5] = {
        {"Junior", 50},
        {"Alerrandro", 40},
        {"Adalberto", 110},
        {"Dalessandro", 120},
        {"Fernandao", 150}
    };

    OrdenaPlayers(Players);

    arq = fopen("top_scores.bin", "wb");
    if (!arq) {
        printf("Erro na criação do arquivo top_scores.bin!\n");
        return;
    }
    fwrite(Players, sizeof(JogadorLeader), 5, arq);
    fclose(arq);
}

// Insere o nome do jogador  (exemplo simples de entrada)
void InsertName(char strnome[50]) {
    char nome[MAX_NOME] = {'\0'};
    char caractere;
    int contaChars = 0;

    while (!IsKeyPressed(KEY_ENTER)) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
        DrawText("PARABENS! VOCE GANHOU", (SCREEN_WIDTH - MeasureText("PARABENS! VOCE GANHOU", 60)) / 2, (SCREEN_HEIGHT - 30) / 2, 60, GREEN);
        DrawText("Aperte enter para voltar ao menu", (SCREEN_WIDTH - MeasureText("PARABENS! VOCE GANHOU", 60)) / 2, (SCREEN_HEIGHT + 60) / 2, 40, WHITE);
        // Recebe o nome do jogador
        caractere = GetCharPressed();

        if (caractere != '\0' && contaChars < 23) {
            nome[contaChars] = caractere;
            contaChars++;
        }

        if (IsKeyPressed(KEY_BACKSPACE)) {
            nome[contaChars - 1] = '\0';
            if (contaChars > 0)
                contaChars--;
        }

        DrawText(TextFormat("Nome: %s", nome), 45, 45, 40, RAYWHITE);
        EndDrawing();
    }
 strcpy(strnome,nome);
}

// Desenha uma tela com os top 5 jogadores (exibe a leaderboard)
void DesenhaTop5(int *guarda) {
    FILE *arq;
    JogadorLeader Players[5];
    int i, j = 0;
    arq = fopen("top_scores.bin", "rb");
    if (!arq) {
        printf("Erro na abertura do arquivo!\n");
        return;
    }

    int bytesRead = fread(Players, sizeof(JogadorLeader), 5, arq);
    fclose(arq);

    if (bytesRead != 5) {
        printf("Erro ao ler dados do arquivo ou o arquivo está incompleto.\n");
        return;
    }

    Rectangle exitButton = {SCREEN_WIDTH - 150, 20, 130, 90}; // Exit button rectangle

    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);
    DrawText("Leaderboard", (SCREEN_WIDTH / 2 - MeasureText("Leaderboard", 50) / 2), 20, 50, WHITE);

    for (i = 0; i < 5; i++) {
        DrawText(TextFormat("Nome: %s", Players[i].nome), 15, 100 + j, 30, WHITE);
        DrawText(TextFormat("Pontuacao: %d \n", Players[i].points), 400, 100 + j, 30, WHITE);
        j += 40;
    }

    // Draw the exit button
    DrawRectangleRec(exitButton, RED);
    DrawText("Aperte \nenter \npara sair", exitButton.x + 20, exitButton.y + 10, 20, WHITE);

    EndDrawing();
}

// Atualiza o leaderboard com o novo jogador e sua pontuação
void AtualizaTop5(JogadorLeader player1) {
    FILE *arq;
    JogadorLeader Players[5];
    int i, j = 0;

    arq = fopen("top_scores.bin", "rb+");
    if (!arq) {
        printf("Erro na abertura do arquivo!\n");
        return;
    }

    fread(Players, sizeof(JogadorLeader), 5, arq);
    OrdenaPlayers(Players);
    Players[4] = player1;
    OrdenaPlayers(Players);
    fwrite(Players, sizeof(JogadorLeader), 5, arq);
    fclose(arq);
}

// Verifica a colisão com o portão e registra a pontuação do jogador
void HandleGateCollision(Player *player, Rectangle block) {
    char nomejogador[MAX_NOME];
    Vector2 correction = {0, 0};
    FILE *arq;
    JogadorLeader Players[5];

    if (CheckCollisionWithBlock(player->rect, block, &correction)) {
        // Verifica se o arquivo existe
        arq = fopen("top_scores.bin", "rb");
        if (!arq) {
            printf("Arquivo não encontrado. Criando top_scores.bin...\n");
            CriaTop5Jogadores();
            arq = fopen("top_scores.bin", "rb"); // Reabre após criação
        }

        // Lê os jogadores do arquivo
        fread(Players, sizeof(JogadorLeader), 5, arq);
        fclose(arq);

        // Insere o nome do jogador atual
        InsertName(nomejogador);

        JogadorLeader jogadorAtual;
        strcpy(jogadorAtual.nome, nomejogador);
        jogadorAtual.points = player->points;

        // Adiciona o novo jogador no final do array
        Players[4] = jogadorAtual;

        // Ordena os jogadores
        OrdenaPlayers(Players);

        // Reabre o arquivo para sobrescrever os dados
        arq = fopen("top_scores.bin", "wb");
        if (!arq) {
            printf("Erro ao abrir top_scores.bin para escrita!\n");
            return;
        }

        fwrite(Players, sizeof(JogadorLeader), 5, arq);
        fclose(arq);

        // Exibe mensagem e finaliza o jogo
        printf("Parabéns, %s! Sua pontuação de %d foi registrada.\n", nomejogador, jogadorAtual.points);
        player->health = -1;
    }
}

// Percorre o array de caracteres (mapa), cria um retangulo e usa CheckCollisionWithBlock() para determinar se o jogador está colidindo com algum bloco.
void HandlePlayerBlockCollisions(Player *player, char map[MAX_HEIGHT][MAX_WIDTH], int rows, int cols, float blockSize) {
    player->isGrounded = false;

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            Rectangle block = {x * blockSize, y * blockSize, blockSize, blockSize};

            if (map[y][x] == 'B') {
                HandleBlockCollision(player, block);
            }
            else if (map[y][x] == 'O') {
                HandleObstacleCollision(player, block);
            }
            else if (map[y][x] == 'G') {
                HandleGateCollision(player, block);
            }
        }
    }
}

// Chama todas as funções de colisão 1 vez só
void HandleCollisions(Player* player, Enemy* enemies, int enemyCount, Projectile projectiles[MAX_PROJECTILES], char map[MAX_HEIGHT][MAX_WIDTH], int rows, int cols, float blockSize, unsigned currentFrame, float dt, Coin coins[MAX_WIDTH], int *coinCount) {
    HandlePlayerBlockCollisions(player, map, rows, cols, blockSize);
    HandlePlayerEnemyCollision(player, enemies, enemyCount, &currentFrame, dt);
    CheckProjectileEnemyCollision(projectiles, &enemyCount, enemies, player);
    CheckPlayerCoinCollision(player, coins, coinCount);
}

// Atualiza textura que apresenta o jogador conforme movimento
void UpdatePlayerAnimation(Player *player, float *frameTimer, float frameSpeed, int *currentFrame) {
    if (*frameTimer >= frameSpeed) { // Condicional para nao entrar em loop infinito
        *frameTimer = 0.0f;

        if (!player->isGrounded) {  // Jogador está no ar
            *currentFrame = player->isShooting ? 10 : 5; // Animação de pulo
        } else if (player->velocity.x != 0)  // Verifica se jogador está se mexendo horizontalmente (eixo x)
            {

            if (player->isShooting) { // Verifica se jogador está atirando, se sim:
                *currentFrame = (*currentFrame < 6 || *currentFrame > 8) ? 7 : *currentFrame + 1; // Varia entre texturas 6, 7 e 9
            } else {  // Se não está atirando:
                *currentFrame = (*currentFrame < 1 || *currentFrame > 3) ? 2 : *currentFrame + 1; // Varia entre texturas 1, 2 e 3
            }
        } else { // Jogador nao está fazendo nenhum movimento
            *currentFrame = player->isShooting ? 7 : 0; // Animação de estar parado ou parado e atirando
        }
    }
}

// Atualiza estado do jogador frame a frame e chama a função cada vez para trocar textura de acordo com estado
void UpdatePlayerAnimationState(Player *player, float *frameTimer, float frameSpeed, unsigned *currentFrame, Rectangle *frameRec, int frameWidth) {
    *frameTimer += GetFrameTime(); // Pega tempo desde o último frame
    UpdatePlayerAnimation(player, frameTimer, frameSpeed, currentFrame); // Aplica textura ao jogador
    frameRec->x = frameWidth * (*currentFrame);
    frameRec->width = player->facingRight ? -frameWidth : frameWidth;
}

// Verifica vida do jogador e, se abaixo de 0, encerra o jogo
int isPlayerDead(Player *player) {
    if (player->health < 0) {
        return 1;
    } else {
        return 0;
    }
}

int BeginGame(Player *player,
             Texture2D infmanTex,
             Rectangle frameRec,
             float *frameTimer,
             unsigned *currentFrame,
             Camera2D camera,
             float frameSpeed,
             float gravity,
             float playerSpeed,
             float jumpForce,
             float enemySpeedX,
             float enemySpeedY,
             float enemyOffset,
             float projectileWidth,
             float projectileHeight,
             float projectileSpeed,
             char map[MAX_HEIGHT][MAX_WIDTH],
             int rows,
             int cols,
             Coin coins[MAX_WIDTH],
             int *coinCount,
             Enemy enemies[MAX_WIDTH],
             int enemyCount,
             Projectile projectiles[MAX_PROJECTILES],
             Texture2D background,
             Texture2D blockTexture,
             Texture2D obstacleTexture,
             Texture2D gateTexture,
             Texture2D enemiesTexture,
             Texture2D heartTexture,
             int frameWidth,
             int *guarda,
             Texture2D enemyTex,
             Rectangle enemyFrameRec
            )
{

    float dt = GetFrameTime();

    ApplyGravity(player, gravity, dt);

    if(!isPlayerDead(player)) {

    UpdatePlayerAnimationState(player, frameTimer, frameSpeed, currentFrame, &frameRec, frameWidth);
    HandleRespawn(player, SCREEN_HEIGHT);

    // Movimento
    MovePlayer(player, playerSpeed, jumpForce, dt);
    MoveCamera(&camera, player);
    MoveEnemies(enemies, enemyCount, dt);
    MoveProjectiles(projectiles, dt, player, SCREEN_WIDTH, map, rows, cols, BLOCK_SIZE);

    // Outros
    CreateProjectile(player, projectiles, projectileWidth, projectileHeight, projectileSpeed, dt);
    HandleCollisions(player, enemies, enemyCount, projectiles, map, rows, cols, BLOCK_SIZE, *currentFrame, dt, coins, coinCount);

    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode2D(camera);

    // Renderiza o fundo atrás do jogador
    RenderBackground(background, rows, cols);

    // Renderiza jogador
    DrawTexturePro(infmanTex, frameRec, player->rect, (Vector2) {0, 0}, 0.0f, WHITE);

    // Renderiza mapa e elementos dinâmicos
    RenderCoins(coins, *coinCount);
    RenderMap(map, rows, cols, BLOCK_SIZE, blockTexture, obstacleTexture, gateTexture);
    RenderProjectiles(projectiles);
    RenderEnemies(enemies, enemyCount, BLOCK_SIZE, enemiesTexture, &enemyFrameRec, frameTimer, currentFrame);

    EndMode2D();

    // Interface
    int heartX = 85;
    int heartY = 37;
    float heartWidth = 30.0f;
    float heartHeight = 30.0f;

    for (int i = 0; i < player->health; i++) {
        Rectangle destRect = { heartX + i * (heartWidth + 5), heartY, heartWidth, heartHeight };
        DrawTexturePro(heartTexture, (Rectangle) {0, 0, heartTexture.width, heartTexture.height}, destRect, (Vector2){0, 0}, 0.0f, WHITE);
    }

    int textX = 10;
    int textY = 40;
    int textHeight = 20;
    int textWidth = MeasureText(TextFormat("Health:"), textHeight);
    DrawRectangle(textX - 5, textY - 5, textWidth + 10, textHeight + 10, BLACK);
    DrawText(TextFormat("Health:"), textX, textY, 20, WHITE);

    int pointsX = 10;
    int pointsY = 70;
    int pointsHeight = 20;
    int pointsWidth = MeasureText(TextFormat("Points: %d", player->points), textHeight);
    DrawRectangle(pointsX - 5, pointsY - 5, pointsWidth + 10, pointsHeight + 10, BLACK);
    DrawText(TextFormat("Points: %d", player->points), 10, 70, textHeight, WHITE);

    EndDrawing();

    *guarda = 1;
    } else {
        *guarda = 0;

        int test = InitializeEnemies(map, rows, cols, enemies, BLOCK_SIZE, enemySpeedX, enemySpeedY, enemyOffset);

        if(player->points < 0) {
            DesenhaTelaFinal();
        }

        player->health = 3;
        player->points = 0;
        player->position = player->spawnPoint;
        WaitTime(0.1);
    }
}

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "INF-MAN");
    InitAudioDevice();
    // World control variables
    float gravity = 800.0;
    float playerSpeed = 200.0;
    float jumpForce = -300.0;

    float enemySpeedX = 150.0;
    float enemySpeedY = 50.0;
    float enemyOffset = 200.0f;

    float projectileWidth = 20.0;
    float projectileHeight = 10.0;
    float projectileSpeed = 400.0;

    float frameSpeed = 0.15f;
    int guarda = 0;
    Music music = LoadMusicStream("musica_jogo.wav");
    // Load map
    int rows, cols;
    char map[MAX_HEIGHT][MAX_WIDTH];
    LoadMap("map.txt", map, &rows, &cols);
    if (rows == 0 || cols == 0) {
        CloseWindow();
        return 1;
    }

    // Initialize player
    Player player = InitializePlayer();
    if (!FindPlayerSpawnPoint(map, rows, cols, &player)) {
        CloseWindow();
        return 1;
    }

    // Load textures
    Texture2D background = LoadTexture("background.png");
    Texture2D blockTexture = LoadTexture("tile1.png");
    Texture2D obstacleTexture = LoadTexture("spike.png");
    Texture2D gateTexture = LoadTexture("gate.png");
    Texture2D enemiesTexture = LoadTexture("enemies.png");
    Texture2D heartTexture = LoadTexture("heart.png");
    Texture2D initializeTexture = LoadTexture("inf_man.png");

    Texture2D infmanTex;
    Rectangle frameRec;
    int frameWidth;

    Texture2D enemyTex;
    Rectangle enemyFrameRec;
    int enemyFrameWidth;

    InitializePlayerTextureAndAnimation(&infmanTex, &frameRec, &frameWidth, &enemyTex, &enemyFrameRec, &enemyFrameWidth);

    Camera2D camera = InitializeCamera(&player);
    float frameTimer = 0.0f;
    unsigned currentFrame = 0;

    Coin coins[MAX_WIDTH];
    int coinCount = InitializeCoins(map, rows, cols, coins, BLOCK_SIZE);

    Enemy enemies[MAX_WIDTH];
    int enemyCount = InitializeEnemies(map, rows, cols, enemies, BLOCK_SIZE, enemySpeedX, enemySpeedY, enemyOffset);

    Projectile projectiles[MAX_PROJECTILES];
    InitializeProjectiles(projectiles);

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        UpdateMusicStream(music);
        SetMusicVolume(music,0.5);
        float dt = GetFrameTime();
        switch (guarda) {
            case 0:
            guarda = Menu();
            break;
            case 1:
                BeginGame(&player, infmanTex, frameRec, &frameTimer, &currentFrame, camera, frameSpeed,
                                        gravity, playerSpeed, jumpForce, enemySpeedX, enemySpeedY, enemyOffset,
                                        projectileWidth, projectileHeight, projectileSpeed, map, rows, cols,
                                        coins, &coinCount, enemies, enemyCount, projectiles, background,
                                        blockTexture, obstacleTexture, gateTexture, enemiesTexture, heartTexture,
                                        frameWidth, &guarda, enemyTex, enemyFrameRec);

                break;
            case 2: {
                    FILE *arq = fopen("top_scores.bin", "rb");
                    if (!arq) {
                        CriaTop5Jogadores();  // Cria o arquivo caso não exista
                    }
                    else {
                        DesenhaTop5(&guarda);// Exibe o leaderboard
                        if(IsKeyPressed(KEY_ENTER)) {
                            guarda = 0;
                        } else {
                            guarda = 2;
                        }
                    }
                    break;
                }
            case 3: {
                StopMusicStream(music);
                CloseAudioDevice();
                CloseWindow();
                return 0;
            }
        }
    }
    return 0;
}
