# Ghost Piece: A Tetris Image Tool

This is a small standalone C applicaction for creating tetris field images for notes and guides.
Unlike other tools, this displays connectivity, and can show ghost pieces for additional options for notation.

## Usage

* Place tiles with left click.
* Delete tiles with right click.
* Place ghost tiles by holding shift.
* Click and drag to place a connected piece.
* Change tile colours by scrolling.
* Export an image with enter, this saves to the application's folder.
* Hold shift when export to automatically crop the image.

## References:

All references / libraries in this project are single file libraries for ease of use, and more stable compilation. Their original sources, and relevant license information can be found here:

### One Line Coder's PixelGameEngine for rendering:

> https://github.com/OneLoneCoder/olcPixelGameEngine

Additionally, I have included a couple of additional patches for image export, and bug fixing:

> https://github.com/OneLoneCoder/olcPixelGameEngine/issues/260#issuecomment-960624096
> https://github.com/OneLoneCoder/olcPixelGameEngine/issues/273#issue-1134228265

### STB for image exporting:

> https://github.com/nothings/stb