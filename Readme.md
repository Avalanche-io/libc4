# libc4 

What is C4?

C4 stands for Cinema Content Creation Cloud which is an open framework for distributed media production. Media as in movies, tv, music, broadcast, etc. Distributed as in studios, editors, post production locations that are in many locations around the world, and distributed as in remote storage and compute (aka the cloud).

Though it started in media C4 is useful for all kinds of distributed data problems.  In particular the C4 identification system (C4 ID), an international standard published by SMPTE, is a universally unique ID that provides a solution to distributed multi-party consensus. 

Think source code management / revision control for huge media files distributed around the world.

## Docs

See [INSTALL.md](INSTALL.md) for build instructions.

See [docs/C4.md](docs/C4.md) for documentation on usage, and example code.

### C Implementation

`libc4` is a c library based on the golang reference implementation found at github.com/Avalanche-io/c4.  

At the moment only the core C4 ID features are implemented. This includes functions for generating and parsing C4 IDs, C4 Digests, and C4 ID Trees.

### Roadmap

`libc4` is in active development so expect regular updates for features and bug fixes. There may be a chance for API changes prior to a 1.0 release, but we are working towards a stable API.

### Contributing

Please submit issues to github if you find bugs, and we welcome pull requests.

Call for platform maintainers. Please open an issue if you would be willing to help us maintain build configurations for other operating systems.

### LICENSE

MIT License see [LICENSE](LICENSE) for more information.

