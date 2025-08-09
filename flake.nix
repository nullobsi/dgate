{
	inputs = {
		nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
		flake-utils.url = "github:numtide/flake-utils";
	};
	outputs = { self, nixpkgs, flake-utils }: flake-utils.lib.eachDefaultSystem (system:
		let
			name = "dgate";
			version = "0.0.1";
			pkgs = import nixpkgs {
				inherit system;
			};
			buildInputs = with pkgs; [
				pkg-config
				ninja
				meson
				libev
			];
		in
		{
			packages.default =
				let
					inherit (pkgs) stdenv lib;
				in
				stdenv.mkDerivation {
					inherit version buildInputs;
					src = self;
					pname = name;
				};
			devShells.default = pkgs.mkShell {
				inherit buildInputs;
			};
		}
	);
	
}
