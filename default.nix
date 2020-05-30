{ pkgs ? import <nixpkgs> {} }:

with pkgs;
stdenv.mkDerivation {
  name = "c-calipto-env";
  buildInputs = [
    cmake
    icu
  ];
}
