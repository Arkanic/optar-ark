pub fn parity(i:u32) -> u32 {
    let mut i = i;
    i ^= i >> 16;
    i ^= i >> 8;
    i ^= i >> 4;
    i ^= i >> 2;
    i ^= i >> 1;

    i & 1
}

pub fn ones(i:u32) -> u32 {
    let mut i = i;
    i -= (i >> 1) & 0x55555555u32;
    i = (i & 0x33333333u32) + ((i & 0xccccccccu32) >> 2);
    i += i >> 4;
    i &= 0x0f0f0f0fu32;
    i += i >> 8;
    i += i >> 16;

    i & 0x3f
}