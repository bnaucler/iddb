# iddb v0.3A
A minimalist identity database to manage your contacts.  

## Written by
Björn Westerberg Nauclér (mail@bnaucler.se) 2017

Compiled and tested on Arch Linux 4.9 (amd64), but should work pretty much anywhere.

## Why iddb?
iddb is written to be lightweight, portable, lightning fast and requiring minimal storage space.

This might be useful in embedded applications or legacy systems.  
However; for most applications you'll find [abook](http://abook.sourceforge.net/) or [khard](https://github.com/scheibler/khard) much more useful.

## Dependencies
* SQLite3

## Installation
`sudo make all install`  
`iddb create`

Unless otherwise specified, the binary will be installed in `/usr/bin` and the database at `$HOME/.iddb.sl3`

## Usage examples
`iddb help` - Prints usage information  
`iddb -f ~/.db create` - Creates or resets database ~/.db  
`iddb import somedir/` - Imports all VCF files in somedir to database  
`iddb new steve steve@company.com company` - Interactively create new contact  
`iddb all steve` - Searches database for steve and prints all information  
`iddb export -d .contacts/ steve` - Searches database for steve and exports VCF files to .contacts/

Add option `-v` for verbose or `-vv` for debug output.

## Contributing
Submit an issue or send a pull request if you feel inclined to get involved.

## Disclaimer
This project is in alpha version, and not recommended for production environments.  
Feel free to explore (and edit!) the code base, but expect bugs.

## License
MIT (do whatever you want)
