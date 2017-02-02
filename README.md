# iddb v0.3A
An identity database to manage your contacts.  

## Written by
Björn Westerberg Nauclér (mail@bnaucler.se) 2017

Compiled and tested on Arch Linux 4.9, but should work pretty much anywhere.

## Why iddb?
iddb is written to be lightweight, portable, lightning fast and requiring minimal storage space.

This might be useful in embedded applications or legacy systems.  
However; for most applications you'll find [abook](http://abook.sourceforge.net/) or [khard](https://github.com/scheibler/khard) much more useful.

## Dependencies
* SQLite3

## Installation
For now, just run `make` and execute the binary from source dir.  
Proper installation will come later.

## Usage examples
`iddb help` - Prints usage information  
`iddb create` - Creates or resets the database  
`iddb new` - Interactively create new contact  
`iddb import somedir/` - Imports all VCF files in somedir to database  
`iddb all steve` - Searches database for steve and prints all information  
`iddb export steve` - Searches database for steve and exports VCF files

Add option `-v` for verbose or `-vv` for debug output.

## Contributing
Submit an issue or send a pull request if you feel inclined to get involved.

## Disclaimer
This project is in alpha version, and not recommended for production environments.  
Feel free to explore (and edit!) the code base, but expect bugs.

## License
MIT (do whatever you want)
