<div align="center">
<pre>
██╗    ██╗ ██████╗ ███████╗████████╗
██║    ██║██╔════╝ ██╔════╝╚══██╔══╝
██║ █╗ ██║██║  ███╗█████╗     ██║   
██║███╗██║██║   ██║██╔══╝     ██║   
╚███╔███╔╝╚██████╔╝███████╗   ██║   
 ╚══╝╚══╝  ╚═════╝ ╚══════╝   ╚═╝   
A minimalist* wget implementation in C
(*see features section)

</pre>
</div>

## Installation

git clone this repo

## Instructions

use make to build the app

## Usage

```sh
./wget [-p PORT] [-d DIRECTORY | -o OUTPUT FILE] URL
```

## Features
- Download websites
- Download files
- Specify output directory or output file; if left unspecified it will get printed to the terminal
- Handle responses according to HTTP/1.1 standard (yet to implement 302)
- NOTE: https is as of now NOT securely implemented

## Limitations
- HTTP/1.1
- IPv4

## Meta
Ivan Cankov - ivanecankov@gmail.com

Distributed under the MIT license. See `LICENSE` for more information.

[https://github.com/Ikurooo/](https://github.com/Ikurooo/)

## Contributing
At this time not allowing any external contributors
