# TDO - your best todo resolver tool
A CLI tool to manage and export your code annotations (TODO/FIXME).

![tdo demo](./demo.gif)

## Usage

### How to build

You must have a Linux or Mac os device with make and gcc installed.

```
git clone https://github.com/theStrangeAdventurer/tdo-resolver <your-dir>

cd <your-dir>

make

sudo mv ./build/tdo /usr/local/bin/

tdo --help
```

### View mode
Browse your todos interactively and open them in your editor (`EDITOR` env var) by pressing Space (␣)
```sh
# Run in view mode (with TUE)
./tdo view --dir <your-project-dir>

# Run in export mode (exports todos in any json file on your filesystem)
./tdo export --dir <your-project-dir> # will safe to default file <your-project-dir>/tdo_export.json
# Or with explicitly --to option pass
```

### Export mode
Export your project annotations to JSON:
```sh
./tdo export --dir <your-project-dir> # will safe to default file <your-project-dir>/tdo_export.json
cat <your-project-dir>/tdo_export.json | jq .data
```
### Example output

```json
[
 {
    "title": "TODO: Your todo title",
    "surround_content_before": "<escaped-lines-of-code-before>",
    "surround_content_after": "<escaped-lines-of-code-after>",
    "path": "/full/path/to/file.ext",
    "line": <line-number>
  },
  {
    ...
  }
]
```

## TroubleShooting
Rendering Issues on macOS

Some terminal fonts may display symbols incorrectly. For best results:
```
# Alacritty terminal example 
font:
  normal:
    family: Menlo
    style: Regular
  size: 14.0
```
