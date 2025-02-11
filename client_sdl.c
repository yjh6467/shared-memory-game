#include "client_sdl.h"

// sudo apt-get install libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev
/*
 * client_common.h에 선언된 내용
 * extern int pipe_client_to_sdl[2];
 * extern int pipe_sdl_to_client[2];
 * extern GameData *dataptr;
 * extern int player_index;
 * 오류 case 제외하고 pipe close 금지
*/

SDL_Window* WIN = NULL;
SDL_Renderer* REN = NULL;

TTF_Font* FONT_DEFAULT = NULL;
SDL_Color COLOR_WHITE = {255, 255, 255, 255};

SDL_Texture* TEX_BOARD = NULL;
SDL_Texture* TEX_GUARD = NULL;
SDL_Texture* TEX_P1_RIGHT = NULL;
SDL_Texture* TEX_P1_LEFT = NULL;
SDL_Texture* TEX_P2_RIGHT = NULL;
SDL_Texture* TEX_P2_LEFT = NULL;

const int TEX_PLAYER_WIDTH = 160, TEX_PLAYER_HEIGHT = 300;
const int SCREEN_WIDTH = 1280, SCREEN_HEIGHT = 720;
Vector2 stage_position[MAP_SIZE];

// 비블로킹 pipe reading 할 때 이전 내용 저장용 변수
char client_msg[MSG_SIZE];

bool init_sdl(const char* title)
{
    // 비디오 서브시스템 초기화 지시
    if (SDL_Init(SDL_INIT_VIDEO) == -1)
    {
        fprintf(stderr, "SDL 초기화 오류: %s\n", SDL_GetError());
        return false;
    }

    WIN = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT,
                           SDL_WINDOW_SHOWN);
    if (WIN == NULL)
    {
        fprintf(stderr, "창 생성 실패: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    REN = SDL_CreateRenderer(WIN, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (REN == NULL)
    {
        fprintf(stderr, "렌더러 생성 실패: %s\n", SDL_GetError());
        SDL_DestroyWindow(WIN);
        SDL_Quit();
        return false;
    }

    if (TTF_Init() == -1)
    {
        fprintf(stderr, "TTF 초기화 오류: %s\n", TTF_GetError());
        return false;
    }

    if (IMG_Init(IMG_INIT_PNG) == 0)
    {
        fprintf(stderr, "IMG 초기화 오류: %s\n", IMG_GetError());
        return false;
    }

    // src matching
    strcpy(client_msg, "DEBUG");

    FONT_DEFAULT = TTF_OpenFont("./src/Pretendard.ttf", 24);
    if (FONT_DEFAULT == NULL)
    {
        fprintf(stderr, "폰트 로드 실패: %s\n", TTF_GetError());
        cleanup();
        return false;
    }

    TEX_BOARD = load_texture("./src/board.png");
    if (TEX_BOARD == NULL)
    {
        fprintf(stderr, "./src/board.png 로드 실패: %s\n", IMG_GetError());
        cleanup();
    }

    TEX_GUARD = load_texture("./src/guard.png");
    if (TEX_GUARD == NULL)
    {
        fprintf(stderr, "./src/guard.png 로드 실패: %s\n", IMG_GetError());
        cleanup();
    }

    TEX_P1_LEFT = load_texture("./src/p1-left.png");
    if (TEX_P1_LEFT == NULL)
    {
        fprintf(stderr, "./src/p1-left.png 로드 실패: %s\n", IMG_GetError());
        cleanup();
    }

    TEX_P1_RIGHT = load_texture("./src/p1-right.png");
    if (TEX_P1_RIGHT == NULL)
    {
        fprintf(stderr, "./src/p1-right.png 로드 실패: %s\n", IMG_GetError());
        cleanup();
    }

    TEX_P2_LEFT = load_texture("./src/p2-left.png");
    if (TEX_P2_LEFT == NULL)
    {
        fprintf(stderr, "./src/p2-left.png 로드 실패: %s\n", IMG_GetError());
        cleanup();
    }

    TEX_P2_RIGHT = load_texture("./src/p2-right.png");
    if (TEX_P2_RIGHT == NULL)
    {
        fprintf(stderr, "./src/p2-right.png 로드 실패: %s\n", IMG_GetError());
        cleanup();
    }

    return true;
}

void assign_stage_position()
{
    const int start_y = 660;
    const int dy = 170;
    bool is_reversed = false; // false: left -> right

    for (int line = 0; line < 4; line++)
    {
        int start_x;
        if (is_reversed) start_x = 1015;
        else start_x = 265;

        for (int column = 0; column < 6; column++)
        {
            int dx = 150;
            if (is_reversed) dx *= -1;

            stage_position[line * 7 + column].x = start_x + column * dx;
            stage_position[line * 7 + column].y = start_y - line * dy;
        }

        if (is_reversed)
            stage_position[line * 7 - 1].x = 1060;
        else
            stage_position[line * 7 - 1].x = 205;
        stage_position[line * 7 - 1].y = start_y - 85 - (line - 1) * dy;
        is_reversed = is_reversed ? false : true;
    }

    for (int line = 0; line < 4; line++)
    {
        for (int column = 0; column <= 6; column++)
        {
            printf("[SDL] stage %d => %d, %d\n", line * 7 + column, stage_position[line * 7 + column].x,
                   stage_position[line * 7 + column].y);
        }
    }
}

void run_sdl()
{
    // 로케일 설정, UTF-8 인코딩 사용
    setlocale(LC_ALL, "");
    printf("[SDL] 프로세스 시작\n");

    if (init_sdl("Octopus Game") == false)
    {
        perror("SDL 초기화 오류");
        exit(EXIT_FAILURE);
    }

    assign_stage_position();
    printf("[SDL] 준비 완료\n");

    // 입력 받기 전 화면 갱신
    SDL_RenderClear(REN);
    render_texture(TEX_BOARD, 0, 0);
    render_texture(TEX_P1_RIGHT, 100, 100);
    render_texture(TEX_P2_RIGHT, 300, 100);
    render_text("게임 준비 중...", 300, 360, COLOR_WHITE);
    SDL_RenderPresent(REN);

    SDL_Event e;
    bool quit = false;
    while (!quit)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                quit = true;
            if (e.type == SDL_MOUSEBUTTONDOWN)
            {
                // 클릭 시 파이프 입력
                printf("[SDL] Clicked\n");
                write_to_client("[MOUSE]");
            }
        }

        // 100ms 단위로 Update
        update();
        usleep(1000 * 100);
    }
    cleanup();
}


void render_text(const char* message, int x, int y, SDL_Color color)
{
    // SDL_ttf에서 UTF-8 문자열 렌더링
    SDL_Surface* surface = TTF_RenderUTF8_Solid(FONT_DEFAULT, message, color);
    if (!surface)
    {
        fprintf(stderr, "TTF_RenderUTF8_Solid 오류: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(REN, surface);
    if (!texture)
    {
        fprintf(stderr, "SDL_CreateTextureFromSurface 오류: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }
    SDL_Rect dest = {x, y, surface->w, surface->h};

    SDL_FreeSurface(surface);

    SDL_RenderCopy(REN, texture, NULL, &dest);
    SDL_DestroyTexture(texture);
}

SDL_Texture* load_texture(const char* file)
{
    SDL_Texture* texture = IMG_LoadTexture(REN, file);
    if (texture == NULL)
    {
        fprintf(stderr, "이미지 로드 실패: %s\n", IMG_GetError());
    }
    return texture;
}

void render_texture(SDL_Texture* tex, int x, int y)
{
    SDL_Rect dst;
    dst.x = x;
    dst.y = y;
    SDL_QueryTexture(tex, NULL, NULL, &dst.w, &dst.h);
    SDL_RenderCopy(REN, tex, NULL, &dst);
}

void cleanup()
{
    TTF_CloseFont(FONT_DEFAULT);
    TTF_Quit();

    SDL_DestroyTexture(TEX_BOARD);
    SDL_DestroyTexture(TEX_GUARD);
    SDL_DestroyTexture(TEX_P1_LEFT);
    SDL_DestroyTexture(TEX_P2_LEFT);
    SDL_DestroyTexture(TEX_P1_RIGHT);
    SDL_DestroyTexture(TEX_P2_RIGHT);
    IMG_Quit();

    SDL_DestroyRenderer(REN);
    SDL_DestroyWindow(WIN);
    SDL_Quit();
}

void update()
{
    SDL_RenderClear(REN); // 이전 렌더링 내용 지우기
    render_texture(TEX_BOARD, 0, 0);

    // 플레이어 인덱스 + 위치 고려한 렌더링
    if (player_index == 0)
    {
        render_player(1);
        render_player(0);
    }
    else
    {
        render_player(0);
        render_player(1);
    }

    char* buffer;
    // 두 글자 이하는 출력 무시
    if (read_from_client(buffer) > 0 && strlen(buffer) > 1)
        strcpy(client_msg, buffer);
    render_text(client_msg, 300, 360, COLOR_WHITE);
    SDL_RenderPresent(REN); // 새 내용 화면에 표시
}

void render_player(int idx)
{
    int p_pos = dataptr->player_position[idx];
    int p_x = stage_position[p_pos].x;
    int p_y = stage_position[p_pos].y;

    bool is_reversed;

    if ((p_pos / 7) % 2 == 0) is_reversed = false;
    else is_reversed = true;

    if (idx == 0 && is_reversed)
        render_texture(TEX_P1_LEFT, p_x - TEX_PLAYER_WIDTH / 2,
                       p_y - TEX_PLAYER_HEIGHT);
    else if (idx == 0 && !is_reversed)
        render_texture(TEX_P1_RIGHT, p_x - TEX_PLAYER_WIDTH / 2,
                       p_y - TEX_PLAYER_HEIGHT);
    else if (idx == 1 && is_reversed)
        render_texture(TEX_P2_LEFT, p_x - TEX_PLAYER_WIDTH / 2,
                       p_y - TEX_PLAYER_HEIGHT);
    else
        render_texture(TEX_P2_RIGHT, p_x - TEX_PLAYER_WIDTH / 2, p_y - TEX_PLAYER_HEIGHT);
}

void write_to_client(char* message)
{
    if (write(pipe_sdl_to_client[1], message, MSG_SIZE) == -1)
    {
        perror("[SDL] pipe write failed");
        exit(EXIT_FAILURE);
    }
}

int read_from_client(char* buffer)
{
    ssize_t count = read(pipe_client_to_sdl[0], buffer, MSG_SIZE);
    if (count == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        // 진짜 오류일 경우
        perror("[SDL] pipe read failed");
        cleanup(); // 리소스 정리 후 종료
        exit(EXIT_FAILURE);
    }

    return count;
}
