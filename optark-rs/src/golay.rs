use crate::parity;

// for each face in the dodecahedron there is a list of adjacent faces
static DODECAHEDRON:[[i32; 5]; 12] = [
    [2,  3,  4,  5,  6],
	[1,  3,  6,  7,  8],
	[1,  2,  4,  8,  9],
	[1,  3,  5,  9, 10],
	[1,  4,  6, 10, 11],
	[1,  2,  5,  7, 11],
	[2,  6,  8, 11, 12],
	[2,  3,  7,  9, 12],
	[3,  4,  8, 10, 12],
	[4,  5,  9, 11, 12],
	[5,  6,  7, 10, 12],
	[7,  8,  9, 10, 11]
];

pub fn golay_generate() -> Vec<u32> {
    let mut parities:[u32; 12] = [0xfff; 12];
    for i in 0..12usize {
        DODECAHEDRON[i].map(|f| parities[i] ^= 1u32 << (f - 1));
    }

    let mut codes:Vec<u32> = Vec::with_capacity(4096);
    for input in 0..4096usize {
        let mut prty = 0u32;
        for p in parities.into_iter() {
            prty <<= 1;
            prty |= parity::parity(input as u32 & p);
        }

        let codeword = ((input as u32) << 12) | prty;
        let n_ones = parity::ones(codeword);
        codes.push(codeword);

        assert!(n_ones == 0 || n_ones == 8 || n_ones == 12 || n_ones == 16 || n_ones == 24);
    }

    return codes;
}