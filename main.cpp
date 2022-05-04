
// Tell the pixelGameEngine to use stb_image.h
// This allows for saving sprites to files.
#define OLC_IMAGE_STB
#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"
#include <time.h>



constexpr int TILE_WIDTH = 32;
//constexpr int FAUX_3D_SIZE = 8;
//constexpr int BORDER_THICKNESS = 6;

constexpr int SPACING_HORIZONTAL = 128;
constexpr int SPACING_VERTICAL   = 32;

constexpr int GRID_WIDTH  = 10;
constexpr int GRID_HEIGHT = 8;

constexpr int FULL_SIZE_X = 2 * SPACING_HORIZONTAL + TILE_WIDTH * GRID_WIDTH;
constexpr int FULL_SIZE_Y = 2 * SPACING_VERTICAL   + TILE_WIDTH * (GRID_HEIGHT + 1);

enum TILES {
	EMPTY = 0, // TRANSPARENT
	GARBAGE = 1, // GREY
	GARBAGE_LIGHT = 2, // WHITE

	I = 3, // CYAN
	O = 4, // YELLOW
	T = 5, // PURPLE
	L = 6, // ORANGE
	J = 7, // BLUE
	S = 8, // GREEN
	Z = 9, // RED
	
	INVALID
};

enum TILE_FLAGS {
	CONNECT_UP = 1,
	CONNECT_DOWN = 2,
	CONNECT_LEFT = 4,
	CONNECT_RIGHT = 8,
	GHOST = 16,
	HIGHLIGHT = 32,
	JUST_SET = 64,
};
constexpr int CONNECT_MASK = 15;

enum PIECE_ROTATIONS {
	UP = 0,
	RIGHT = 1,
	DOWN = 2,
	LEFT = 3
};

// Could pack these into just one int array, using a bit-mask for isolating the colour.
uint32_t tilemap_tiles[GRID_WIDTH * GRID_HEIGHT];
uint32_t tilemap_flags[GRID_WIDTH * GRID_HEIGHT];
int highlight_tile_x = -1;
int highlight_tile_y = -1;

int last_highlight_tile_x = -1;
int last_highlight_tile_y = -1;

int pallette_colour = TILES::GARBAGE;
bool pallette_ghost = false;

olc::Sprite spriteTileMap;
olc::Sprite spriteTileSheet;
olc::Sprite spriteGhostSheet;
olc::Sprite spriteBackground;

std::unique_ptr<olc::Decal> decalTileMap;
std::unique_ptr<olc::Decal> decalTileSheet;
std::unique_ptr<olc::Decal> decalGhostSheet;
std::unique_ptr<olc::Decal> decalBackground;

// @TODO: Feature: Undo stack.
// @TODO: Split code into multiple files? - Only 500 lines, probably not justifiable.

class GhostPieceEngine : public olc::PixelGameEngine
{
public:
	GhostPieceEngine() {
		sAppName = "Ghost Piece - Tetris Image Generator";
	}
	
	// Called once at setup time.
	bool OnUserCreate() override
	{
		spriteTileMap = olc::Sprite(TILE_WIDTH * GRID_WIDTH, TILE_WIDTH * (GRID_HEIGHT + 1));
		spriteTileSheet = olc::Sprite("./resources/connected_tiles.png");
		spriteGhostSheet = olc::Sprite("./resources/connected_ghosts.png");
		spriteBackground = olc::Sprite("./resources/background.png");

		decalTileMap = std::make_unique<olc::Decal>(&spriteTileMap);
		decalTileSheet = std::make_unique<olc::Decal>(&spriteTileSheet);
		decalGhostSheet = std::make_unique<olc::Decal>(&spriteGhostSheet);
		decalBackground = std::make_unique<olc::Decal>(&spriteBackground);

		drawToTileMap();

		return true;
	}

	// Called once per frame.
	bool OnUserUpdate(float fElapsedTime) override
	{
		update();
		draw();
		return true;
	}

	void update() {
		// Find what tile the mouse is over.
		int mx = GetMouseX();
		int my = GetMouseY();
		// Code for getting tile divides towards 0, so bounds check must be done first, not on the result.
		// Offest by one vertically to account for room in tilemap sprite for top face sprites.
		if (mx < SPACING_HORIZONTAL || mx >= SPACING_HORIZONTAL + TILE_WIDTH * GRID_WIDTH || my < SPACING_VERTICAL + TILE_WIDTH || my >= SPACING_VERTICAL + TILE_WIDTH * (GRID_HEIGHT + 1)) {
			highlight_tile_x = -1;
			highlight_tile_y = -1;
		}
		else
		{
			highlight_tile_x = ((mx - SPACING_HORIZONTAL) / TILE_WIDTH);
			highlight_tile_y = ((my - SPACING_VERTICAL) / TILE_WIDTH) - 1;

			// Only update grid if a mouse button has just been pressed OR if the highlighted tile has just changed.
			// This should prevent the sprite being updated and resent to the grapics card more than is necessary, which has a performance penalty.
			if (GetMouse(0).bPressed || GetMouse(1).bPressed || last_highlight_tile_x != highlight_tile_x || last_highlight_tile_y != highlight_tile_y)
			{
				if (GetMouse(0).bHeld) {
					// Place tiles, and break connections for overwritten tiles.
					if (pallette_ghost) {
						tilemap_flags[highlight_tile_x + highlight_tile_y * GRID_WIDTH] |= TILE_FLAGS::GHOST;
					} else {
						tilemap_flags[highlight_tile_x + highlight_tile_y * GRID_WIDTH] &= ~TILE_FLAGS::GHOST;
					}
					// reset connection flags for tile just set.
					tilemap_flags[highlight_tile_x + highlight_tile_y * GRID_WIDTH] &= ~CONNECT_MASK;
					disconnectAdjacentTiles(highlight_tile_x, highlight_tile_y);
					tilemap_flags[highlight_tile_x + highlight_tile_y * GRID_WIDTH] |= TILE_FLAGS::JUST_SET;
					tilemap_tiles[highlight_tile_x + highlight_tile_y * GRID_WIDTH] = pallette_colour;
					drawToTileMap();
				}
				else if (GetMouse(1).bHeld) {
					// Erase tiles, and break connections for cleared tiles.
					disconnectAdjacentTiles(highlight_tile_x, highlight_tile_y);
					tilemap_tiles[highlight_tile_x + highlight_tile_y * GRID_WIDTH] = TILES::EMPTY;
					tilemap_flags[highlight_tile_x + highlight_tile_y * GRID_WIDTH] = 0;
					drawToTileMap();
				}
			}
		}
		last_highlight_tile_x = highlight_tile_x;
		last_highlight_tile_y = highlight_tile_y;

		// When left mouse is released, connect all placed tiles together.
		if (GetMouse(0).bReleased) {
			for (int tx = 0; tx < GRID_WIDTH; tx++)
			{
				for (int ty = 0; ty < GRID_HEIGHT; ty++)
				{
					if (tilemap_flags[tx + ty * GRID_WIDTH] & TILE_FLAGS::JUST_SET) {
						if (tx < (GRID_WIDTH - 1) && tilemap_flags[tx + 1 + ty * GRID_WIDTH] & TILE_FLAGS::JUST_SET) {
							// Connect horizontally.
							tilemap_flags[tx + ty * GRID_WIDTH] |= TILE_FLAGS::CONNECT_RIGHT;
							tilemap_flags[tx + 1 + ty * GRID_WIDTH] |= TILE_FLAGS::CONNECT_LEFT;
						}
						if (ty < (GRID_HEIGHT - 1) && tilemap_flags[tx + (ty + 1) * GRID_WIDTH] & TILE_FLAGS::JUST_SET) {
							// Connect vertically.
							tilemap_flags[tx + ty * GRID_WIDTH] |= TILE_FLAGS::CONNECT_DOWN;
							tilemap_flags[tx + (ty + 1) * GRID_WIDTH] |= TILE_FLAGS::CONNECT_UP;
						}
					}
				}
			}

			// Reset the just set flag on all placed pieces.
			for (int tile = 0; tile < GRID_WIDTH * GRID_HEIGHT; tile++)
			{
				tilemap_flags[tile] &= ~TILE_FLAGS::JUST_SET;
			}

			// Redraw the tilemap
			drawToTileMap();
		}

		// Only update pallette if not placing tiles.
		if (!GetMouse(0).bHeld) {
			// Mouse wheel to change pallette colour.
			if (GetMouseWheel() > 0) {
				pallette_colour++;
				if (pallette_colour >= TILES::INVALID) {
					pallette_colour = TILES::EMPTY + 1;
				}
			}
			else if (GetMouseWheel() < 0) {
				pallette_colour--;
				if (pallette_colour <= TILES::EMPTY) {
					pallette_colour = TILES::INVALID - 1;
				}
			}
			// Hold shift to place ghost pieces.
			pallette_ghost = GetKey(olc::Key::SHIFT).bHeld;
		}

		// Middle click to copy colour of existing tile.
		if (GetMouse(2).bPressed) {
			if (highlight_tile_x != -1 && highlight_tile_y != -1) {
				if (tilemap_tiles[highlight_tile_x + highlight_tile_y * GRID_WIDTH] != TILES::EMPTY) {
					pallette_colour = tilemap_tiles[highlight_tile_x + highlight_tile_y * GRID_WIDTH];
				}
			}
		}

		// Enter to export to image.
		if (GetKey(olc::ENTER).bPressed) {
			exportMap();
		}
	}

	void draw() {
		// We should be always painting over everything with decals, so no need to clear the base layer.
		//Clear(olc::DARK_BLUE);

		// Draw background:
		DrawDecal({ 0, 0 }, decalBackground.get());

		// Draw tilemap:
		DrawDecal(olc::vf2d(SPACING_HORIZONTAL, SPACING_VERTICAL), decalTileMap.get());

		// Draw pallette preview:
		olc::Decal* previewDecal;
		if (pallette_ghost) {
			previewDecal = decalGhostSheet.get();
		} else {
			previewDecal = decalTileSheet.get();
		}
		// Active pallette colour
		DrawPartialDecal(
			olc::vf2d(SPACING_HORIZONTAL + (GRID_WIDTH + 1) * TILE_WIDTH, SPACING_VERTICAL + TILE_WIDTH * 3), { TILE_WIDTH * 2, TILE_WIDTH * 2 },
			previewDecal, olc::vf2d(0, pallette_colour * TILE_WIDTH), { TILE_WIDTH, TILE_WIDTH }, olc::WHITE);
		// Previous pallette colour
		DrawPartialDecal(
			olc::vf2d(SPACING_HORIZONTAL + (GRID_WIDTH + 1) * TILE_WIDTH, SPACING_VERTICAL + TILE_WIDTH * 1.5), { TILE_WIDTH, TILE_WIDTH },
			previewDecal, olc::vf2d(0, (pallette_colour + 1) * TILE_WIDTH), { TILE_WIDTH, TILE_WIDTH }, olc::WHITE);
		// Next pallette colour
		DrawPartialDecal(
			olc::vf2d(SPACING_HORIZONTAL + (GRID_WIDTH + 1) * TILE_WIDTH, SPACING_VERTICAL + TILE_WIDTH * 5.5), { TILE_WIDTH, TILE_WIDTH },
			previewDecal, olc::vf2d(0, (pallette_colour - 1) * TILE_WIDTH ), { TILE_WIDTH, TILE_WIDTH }, olc::WHITE);

		// Draw the cursor
		if (highlight_tile_x >= 0 && highlight_tile_y >= 0) {
			// Offest by one vertically to account for room in tilemap sprite for top face sprites.
			DrawPartialDecal(
				olc::vf2d(SPACING_HORIZONTAL + TILE_WIDTH * (highlight_tile_x - 1), SPACING_VERTICAL + TILE_WIDTH * highlight_tile_y), { TILE_WIDTH*3, TILE_WIDTH * 3 },
				previewDecal, olc::vf2d(TILE_WIDTH * (TILES::INVALID + 1), TILE_WIDTH * (TILES::INVALID + 1)), { TILE_WIDTH * 3, TILE_WIDTH * 3 }, olc::WHITE);
		}
	}

	// @OPTIMISATION: Only redraw what must be redrawn, rather than the whole sprite?
	void drawToTileMap() 
	{
		// Select sprite as draw target.
		SetDrawTarget(&spriteTileMap);
		SetPixelMode(olc::Pixel::Mode::APROP);
		Clear(olc::Pixel(255,255,255,0));

		for (int x = 0; x < GRID_WIDTH; x++)
		{
			// Direction reversed for draw order.
			// Limit is y=-1 to allow for top sprites at top of board.
			for (int y = GRID_HEIGHT - 1; y >= -1; y--)
			{
				drawTileToMap(x, y);
			}
		}
		// Reset draw target to screen.
		SetPixelMode(olc::Pixel::Mode::NORMAL);
		SetDrawTarget(static_cast<uint8_t>(0));
		decalTileMap->Update();
	}

	void drawTileToMap(int x, int y) 
	{
		// Safety check on x, y coords.
		// Permits drawing at y = -1 to draw tops of top tiles.
		if (x < 0 || y < -1 || x >= GRID_WIDTH || y >= GRID_HEIGHT) {
			return;
		}

		int colour_index = TILES::EMPTY;
		int connection_index = 0;
		if (y >= 0) {
			colour_index = tilemap_tiles[x + y * GRID_WIDTH];
			connection_index = tilemap_flags[x + y * GRID_WIDTH] & CONNECT_MASK;
		}

		// If not bottom tile, and either empty or ghost, then apply faux 3d for tile beneath.
		if (y < GRID_HEIGHT - 1 && 
			(
				colour_index == TILES::EMPTY ||
				(
					tilemap_flags[x + y * GRID_WIDTH] & TILE_FLAGS::GHOST && 
					!(tilemap_flags[x + y * GRID_WIDTH] & TILE_FLAGS::CONNECT_DOWN)
				)
			)) 
		{
			// Look at next tile down.
			int lower_colour_index = tilemap_tiles[x + (y + 1) * GRID_WIDTH];
			int lower_connection_index = tilemap_flags[x + (y + 1) * GRID_WIDTH] & (TILE_FLAGS::CONNECT_LEFT | TILE_FLAGS::CONNECT_RIGHT);//CONNECT_MASK;//
			lower_connection_index = lower_connection_index >> 2;
			if (lower_colour_index > 0 && lower_colour_index < INVALID) {

				olc::Sprite* sprite;
				if (tilemap_flags[x + (y+1) * GRID_WIDTH] & TILE_FLAGS::GHOST) {
					sprite = &spriteGhostSheet;
				}
				else {
					sprite = &spriteTileSheet;
				}
				DrawPartialSprite(
					olc::vi2d(x * TILE_WIDTH, (y + 1) * TILE_WIDTH),
					sprite,
					olc::vi2d(TILE_WIDTH * lower_colour_index, TILE_WIDTH * (TILES::INVALID + 1 + lower_connection_index)),
					olc::vi2d(TILE_WIDTH, TILE_WIDTH)
				);
			}
		}

		// If tile colour is empty or out of bounds, draw nothing.
		if (colour_index <= 0 || colour_index >= INVALID) {
			return;
		}
	
		olc::Sprite* sprite;
		if (tilemap_flags[x + y * GRID_WIDTH] & TILE_FLAGS::GHOST) {
			sprite = &spriteGhostSheet;
		} else {
			sprite = &spriteTileSheet;
		}
		DrawPartialSprite(
			olc::vf2d(x * TILE_WIDTH, (y + 1) * TILE_WIDTH),
			sprite,
			olc::vf2d(TILE_WIDTH * connection_index, TILE_WIDTH * colour_index),
			olc::vf2d(TILE_WIDTH, TILE_WIDTH)
		);
	}

	// Saves the tile map image to a file.
	// Crops to only tiles that exist if shift is pressed.
	void exportMap() {
		// Build tynamic filename so that each export is a fresh file, rather than overwriting.
		// This is useful for exporting multiple related patterns without needing to manually avoid overwriting each one.
		time_t rawtime;
		time(&rawtime);
		tm timeinfo;
		localtime_s(&timeinfo, &rawtime);
		char filename_buffer[255];

		if (GetKey(olc::SHIFT).bHeld) {
			// Find min and max filled x and y coords on the map, so we know how to crop the image.
			int min_x = GRID_WIDTH - 1;
			int min_y = GRID_HEIGHT - 1;
			int max_x = 0;
			int max_y = 0;
			for (int x = 0; x < GRID_WIDTH; x++) {
				for (int y = 0; y < GRID_HEIGHT; y++) {
					if (tilemap_tiles[x + y * GRID_WIDTH] != TILES::EMPTY) {
						if (x < min_x)
							min_x = x;
						if (y < min_y)
							min_y = y;
						if (x > max_x)
							max_x = x;
						if (y > max_y)
							max_y = y;
					}
				}
			}
			if (min_x <= max_x && min_y <= max_y) {
				int w = max_x - min_x + 1;
				int h = max_y - min_y + 2;

				// Draw the sprite tilemap to a smaller sprite to crop the field, and then save out that new image.
				// Temp sprite is declared on the stack rather than the heap, so no need to manually free it.
				olc::Sprite temp;
				temp = olc::Sprite(TILE_WIDTH * w, TILE_WIDTH * h);
				SetDrawTarget(&temp);
				Clear(olc::BLANK);
				SetPixelMode(olc::Pixel::Mode::APROP);
				
				DrawPartialSprite(
					olc::vi2d(0, 0), 
					&spriteTileMap, 
					olc::vi2d(min_x * TILE_WIDTH, min_y * TILE_WIDTH ), 
					olc::vi2d(w * TILE_WIDTH, h * TILE_WIDTH ));

				SetPixelMode(olc::Pixel::Mode::NORMAL);
				SetDrawTarget(static_cast<uint8_t>(0));

				strftime(filename_buffer, 255, "./out/field_cropped_%Y-%m-%d_%H-%M-%S.png", &timeinfo);
				temp.SaveToFile(filename_buffer);
			}
		} else {
			strftime(filename_buffer, 255, "./out/field_%Y-%m-%d_%H-%M-%S.png", &timeinfo);
			spriteTileMap.SaveToFile(filename_buffer);
		}
	}

	void disconnectAdjacentTiles(int tx, int ty) {
		if (tx < 0 || ty < 0 || tx >= GRID_WIDTH || ty >= GRID_HEIGHT) {
			return;
		}
		if (tx > 0)
			tilemap_flags[tx - 1 + ty * GRID_WIDTH] &= ~TILE_FLAGS::CONNECT_RIGHT;
		if (ty > 0)
			tilemap_flags[tx + (ty - 1) * GRID_WIDTH] &= ~TILE_FLAGS::CONNECT_DOWN;
		if (tx < GRID_WIDTH - 1)
			tilemap_flags[tx + 1 + ty * GRID_WIDTH] &= ~TILE_FLAGS::CONNECT_LEFT;
		if (ty < GRID_HEIGHT - 1)
			tilemap_flags[tx + (ty + 1) * GRID_WIDTH] &= ~TILE_FLAGS::CONNECT_UP;
	}
};

// @TODO: May want to allocate more stuff away from the stack?
int main()
{
	GhostPieceEngine demo;
	bool constructed = demo.Construct(
		FULL_SIZE_X,	// screen width
		FULL_SIZE_Y,	// screen height
		1,		// pixel width
		1,		// pixel height
		false,	// fullscreen
		true	// vsync
	);

	if (constructed)
		demo.Start();
	return 0;
}