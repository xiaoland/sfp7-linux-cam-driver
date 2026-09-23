# Reconstructing the tested kernel source

The tested composition is pinned to [linux-surface/kernel](https://github.com/linux-surface/kernel) commit `57d61aff0b53b089227f5a794363fec829114fc5`, whose ancestor is Linux stable `v6.19.8` (`86818b2e7d9c22225b15f2ae91d3f35c4a07dfd9`). Its starting Git tree is `2ae739c646562a36792549305c19360383e5d82d`. The following source layers are applied in order:

1. 56 patches from [Ruslan Bay's IPU4P work](https://github.com/linux-surface/kernel/pull/163), yielding Git tree `f8c29814a956624c891f1889dc0e760779fbe466`.
2. [thisiscamk's Surface Pro 7 source overlay](https://github.com/thisiscamk/sp7-ipu4-camera), yielding `99184946d2625b8674faabab7a21d98525df5258`.
3. The Surface OV5693 no-binning change, yielding `7fe4572a7046b7915fad6927cb957cbf947e7f44`.
4. `kernel/runtime-final-net.patch`, yielding the exact tested source tree `65c10d4c4f7b651204757b62d608755cc4c21779`.

Use a clean linux-surface checkout at the pinned commit:

```sh
git clone https://github.com/linux-surface/kernel.git linux-sfp7
git -C linux-sfp7 checkout --detach 57d61aff0b53b089227f5a794363fec829114fc5
./scripts/prepare-kernel.sh linux-sfp7
```

The script checks the Git tree after each layer. It stages patches in the checkout but does not build or install a kernel. Use a case-sensitive filesystem for a checkout of the full Linux tree. The final patch is a flattened net result of 128 experimental runtime entries. It is retained for reproducibility and **must be split into logical, attributable changes before an upstream submission**. Its SHA-256 is `f8d9237aa251848e21176e72e602b50bb0f2d82e54ee996a6cf9e04eba260453`.

The tested P1 build used Fedora 42 x86_64, `LOCALVERSION=-sfp7cam.p1.fc42.x86_64`, and the [exact target-derived config](../kernel/p1-fedora42.config) with SHA-256 `2001c9ef43d83f1ba54c02574c5a6257a71affe227fd869b9cb2f66b3c209491`. To compile the same source/config combination without installing it:

```sh
mkdir -p build-p1
cp kernel/p1-fedora42.config build-p1/.config
make -C linux-sfp7 O="$PWD/build-p1" ARCH=x86_64 \
  LOCALVERSION=-sfp7cam.p1.fc42.x86_64 olddefconfig
make -C linux-sfp7 O="$PWD/build-p1" ARCH=x86_64 \
  LOCALVERSION=-sfp7cam.p1.fc42.x86_64 -j"$(nproc)" bzImage modules
```

The [P1 RPM spec](../kernel/packaging/kernel.spec) records the native package recipe with a local `%post` change to preserve the previous GRUB default. Its changelog email was changed to the public GitHub noreply address; the tested spec's SHA-256 was `8fe36028bf79bd114f67be79c681bc85a48ce2673702ba57e3a8c6bb93d5b405`, while this copy is `06b6848b109ba34f440975fcaa7fe273096bfe5441a3f5f800f8c65b5cc6debb`. The package source RPM and signed distribution channel are not hosted here. Bit-for-bit binary reproducibility has not been established. See [status](STATUS.md) for the distinction between source equivalence and runtime validation.

The libcamera series is listed in [`userspace/libcamera-v0.4-runtime-fixes/series`](../userspace/libcamera-v0.4-runtime-fixes/series) and applies to libcamera tag `v0.4.0` in order. Patches 0001–0006 are attributed upstream backports; 0003 adapts hunk context for v0.4.0. The Snapshot patch targets version 48.0.1. Fedora 42 spec overlays record the local package deltas. These are pinned source inputs, not a claim of compatibility with current libcamera or Snapshot releases.
