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

use make to build the server

## Usage

```sh
./wget [-p PORT] [-d DIRECTORY | -o OUTPUT FILE] [-r RECURSION LEVEL] [-g] URL
```

## Features
- Download websites (recursively and with or without all files linked on the site)
- Download small files (yet to implement efficient large file downloads)
- on 16-bit systems up to 64KB; on 32-bit systems up to 4GB; on 64-bit systems up to 16TB (If you have enough system memory)
- Specify output directory or output file; if left unspecified it will get printed to the terminal
- Handle responses according to HTTP/1.1 standard (yet to implement 302)
- NOTE: https is as of now NOT securely implemented

## Meta
Ivan Cankov - ivanecankov@gmail.com

Distributed under the MIT license. See `LICENSE` for more information.

[https://github.com/Ikurooo/](https://github.com/Ikurooo/)

## Contributing
At this time not allowing any external contributors
