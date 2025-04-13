# TDO - your best todo resolver tool
A Cli tool which can help you to manage and export your TODO | FIXME code notes.

## Usage

```sh
# Run in view mode (with TUE)
./tdo view --dir <your-project-dir>

# Run in export mode (exports todos in any json file on your filesystem)
./tdo export --dir <your-project-dir> # will safe to default file <your-project-dir>/tdo_export.json
# Or with explicitly --to option pass

./tdo export --dir <your-project-dir> # will safe to default file <your-project-dir>/tdo_export.json

```

## TroubleShooting

- Broken rendering on MacOS - Known issue on some fonts, to fix this problem you should set the default macos front in terminal

```
# Alacritty terminal example 
font:
  normal:
    family: Menlo
    style: Regular
  size: 14.0
```
