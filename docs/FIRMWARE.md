# Obtain the IPU4P firmware from Microsoft

The Surface Pro 7 camera stack needs `ipu4p_cpd.bin`. This repository does not distribute it because redistribution permission has not been established.

The verified source is [Microsoft's Surface Pro 7 driver download](https://www.microsoft.com/download/details.aspx?id=100419), specifically [`SurfacePro7_Win11_22621_25.090.3489.0.msi`](https://download.microsoft.com/download/b7ea0d32-93fb-4733-b78c-4263b314dbf4/SurfacePro7_Win11_22621_25.090.3489.0.msi). The MSI is 711,032,832 bytes with SHA-256 `16d072797526b6be42a9e2633d760664dcc864163122c8248ba57fc64578d354`.

The member `SurfaceUpdate/camera/cpd_component_signed.bin` is 3,960,832 bytes with SHA-256 `ff2c36cc81a5c726508b22970c2e2538ff06107dc5a72c93401403c227e5157f`. On the tested Fedora system, this exact member was installed as `/usr/lib/firmware/ipu4p_cpd.bin`.

After downloading the MSI yourself, use `msiextract` to inspect and extract it:

```sh
msi=SurfacePro7_Win11_22621_25.090.3489.0.msi
sha256sum "$msi"
msiextract -l "$msi" | grep 'SurfaceUpdate/camera/cpd_component_signed.bin'
mkdir -p surface-msi-extract
msiextract -C surface-msi-extract "$msi"
sha256sum surface-msi-extract/SurfaceUpdate/camera/cpd_component_signed.bin
```

Check both hashes before use. The exact MSI member and hash were also identified in the [linux-surface IPU4 discussion](https://github.com/linux-surface/linux-surface/discussions/1353). A different Surface model or MSI version is outside this verification.
