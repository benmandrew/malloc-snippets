# What is `malloc` doing, anyway?

The programs and captured outputs behind "[What is `malloc` doing, anyway?](https://benmandrew.com/articles/what-is-malloc-doing-anyway)". The post links to the `v1` tag.

## Running

`make` builds a pinned Debian 12 image, runs every program inside it, and writes each output to `output/<name>.txt`. `make -B` reruns everything. On a Linux host with gcc and strace, `make outputs` does the same without Docker.

The committed outputs come from an Intel Xeon E3-1230 v3 running Linux 6.8, and `output/environment.txt` records the versions. This CPU pays for Meltdown and MDS mitigations on every kernel entry, and the `syscall-cost` timings include that cost. Addresses differ on every run. On Apple silicon the container runs as aarch64, and `PLATFORM=linux/amd64` does not help, because emulated x86-64 cannot run `strace`.

## Licence

MIT, see [`LICENSE`](LICENSE).
