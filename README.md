# AYPSG to VGM Converter

AYPSG to VGM Converter is a command-line tool written in C++ that converts AY-3-8910 PSG (Programmable Sound Generator) files into VGM (Video Game Music) format. This tool is particularly useful for retro gaming enthusiasts and developers working with ZX Spectrum or other systems using the AY-3-8910 sound chip.

## Features

- Converts AY-3-8910 PSG files to VGM format.
- Supports custom clock rates and AY chip types.
- Handles PSG commands and timing accurately.
- Outputs VGM files compatible with modern music players and emulators.

## Requirements

- C++14 compatible compiler.
- Standard C++ libraries.

## Usage

aypsg2vgm <input_psg> [output_vgm] [-r rate] [-t ay_type]

## How It Works

1. **VGM Header Generation**: The tool writes a VGM header with the specified clock rate and chip type.
2. **PSG Data Parsing**: Reads PSG commands and translates them into VGM commands.
3. **Timing and Finalization**: Handles delays and writes the EOF offset and total sample count to finalize the VGM file.

## Building the Project

1. Clone the repository or download the source code.
2. Open the project in Visual Studio 2022.
3. Build the project using the provided solution file.

## Notes

- The tool assumes the input ZX Spectrum PSG file is in a valid format.
- The default delay is based on a 50 Hz refresh rate (882 samples per frame).

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

## Acknowledgments

- Special thanks to the VGM format documentation and community.

## Contact

For questions or suggestions, feel free to open an issue or contribute to the project.