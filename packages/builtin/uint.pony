primitive U8 is _UnsignedInteger[U8]
  new create(value: U8 = 0) => compile_intrinsic
  fun tag from[B: (Number & Real[B] val)](a: B): U8 => a.u8()

  fun tag min_value(): U8 => 0
  fun tag max_value(): U8 => 0xFF

  fun next_pow2(): U8 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x + 1

  fun abs(): U8 => this
  fun max(that: U8): U8 => if this > that then this else that end
  fun min(that: U8): U8 => if this < that then this else that end

  fun bswap(): U8 => this
  fun popcount(): U8 => @"llvm.ctpop.i8"[U8](this)
  fun clz(): U8 => @"llvm.ctlz.i8"[U8](this, false)
  fun ctz(): U8 => @"llvm.cttz.i8"[U8](this, false)
  fun bitwidth(): U8 => 8

  fun addc(y: U8): (U8, Bool) =>
    @"llvm.uadd.with.overflow.i8"[(U8, Bool)](this, y)
  fun subc(y: U8): (U8, Bool) =>
    @"llvm.usub.with.overflow.i8"[(U8, Bool)](this, y)
  fun mulc(y: U8): (U8, Bool) =>
    @"llvm.umul.with.overflow.i8"[(U8, Bool)](this, y)

primitive U16 is _UnsignedInteger[U16]
  new create(value: U16 = 0) => compile_intrinsic
  fun tag from[A: (Number & Real[A] val)](a: A): U16 => a.u16()

  fun tag min_value(): U16 => 0
  fun tag max_value(): U16 => 0xFFFF

  fun next_pow2(): U16 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x + 1

  fun abs(): U16 => this
  fun max(that: U16): U16 => if this > that then this else that end
  fun min(that: U16): U16 => if this < that then this else that end

  fun bswap(): U16 => @"llvm.bswap.i16"[U16](this)
  fun popcount(): U16 => @"llvm.ctpop.i16"[U16](this)
  fun clz(): U16 => @"llvm.ctlz.i16"[U16](this, false)
  fun ctz(): U16 => @"llvm.cttz.i16"[U16](this, false)
  fun bitwidth(): U16 => 16

  fun addc(y: U16): (U16, Bool) =>
    @"llvm.uadd.with.overflow.i16"[(U16, Bool)](this, y)
  fun subc(y: U16): (U16, Bool) =>
    @"llvm.usub.with.overflow.i16"[(U16, Bool)](this, y)
  fun mulc(y: U16): (U16, Bool) =>
    @"llvm.umul.with.overflow.i16"[(U16, Bool)](this, y)

primitive U32 is _UnsignedInteger[U32]
  new create(value: U32 = 0) => compile_intrinsic
  fun tag from[A: (Number & Real[A] val)](a: A): U32 => a.u32()

  fun tag min_value(): U32 => 0
  fun tag max_value(): U32 => 0xFFFF_FFFF

  fun next_pow2(): U32 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)
    x + 1

  fun abs(): U32 => this
  fun max(that: U32): U32 => if this > that then this else that end
  fun min(that: U32): U32 => if this < that then this else that end

  fun bswap(): U32 => @"llvm.bswap.i32"[U32](this)
  fun popcount(): U32 => @"llvm.ctpop.i32"[U32](this)
  fun clz(): U32 => @"llvm.ctlz.i32"[U32](this, false)
  fun ctz(): U32 => @"llvm.cttz.i32"[U32](this, false)
  fun bitwidth(): U32 => 32

  fun addc(y: U32): (U32, Bool) =>
    @"llvm.uadd.with.overflow.i32"[(U32, Bool)](this, y)
  fun subc(y: U32): (U32, Bool) =>
    @"llvm.usub.with.overflow.i32"[(U32, Bool)](this, y)
  fun mulc(y: U32): (U32, Bool) =>
    @"llvm.umul.with.overflow.i32"[(U32, Bool)](this, y)

primitive U64 is _UnsignedInteger[U64]
  new create(value: U64 = 0) => compile_intrinsic
  fun tag from[A: (Number & Real[A] val)](a: A): U64 => a.u64()

  fun tag min_value(): U64 => 0
  fun tag max_value(): U64 => 0xFFFF_FFFF_FFFF_FFFF

  fun next_pow2(): U64 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)
    x = x or (x >> 32)
    x + 1

  fun abs(): U64 => this
  fun max(that: U64): U64 => if this > that then this else that end
  fun min(that: U64): U64 => if this < that then this else that end

  fun bswap(): U64 => @"llvm.bswap.i64"[U64](this)
  fun popcount(): U64 => @"llvm.ctpop.i64"[U64](this)
  fun clz(): U64 => @"llvm.ctlz.i64"[U64](this, false)
  fun ctz(): U64 => @"llvm.cttz.i64"[U64](this, false)
  fun bitwidth(): U64 => 64

  fun addc(y: U64): (U64, Bool) =>
    @"llvm.uadd.with.overflow.i64"[(U64, Bool)](this, y)
  fun subc(y: U64): (U64, Bool) =>
    @"llvm.usub.with.overflow.i64"[(U64, Bool)](this, y)
  fun mulc(y: U64): (U64, Bool) =>
    @"llvm.umul.with.overflow.i64"[(U64, Bool)](this, y)

primitive U128 is _UnsignedInteger[U128]
  new create(value: U128 = 0) => compile_intrinsic
  fun tag from[A: (Number & Real[A] val)](a: A): U128 => a.u128()

  fun tag min_value(): U128 => 0
  fun tag max_value(): U128 => 0xFFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF_FFFF

  fun next_pow2(): U128 =>
    var x = this - 1
    x = x or (x >> 1)
    x = x or (x >> 2)
    x = x or (x >> 4)
    x = x or (x >> 8)
    x = x or (x >> 16)
    x = x or (x >> 32)
    x = x or (x >> 64)
    x + 1

  fun abs(): U128 => this
  fun max(that: U128): U128 => if this > that then this else that end
  fun min(that: U128): U128 => if this < that then this else that end

  fun bswap(): U128 => @"llvm.bswap.i128"[U128](this)
  fun popcount(): U128 => @"llvm.ctpop.i128"[U128](this)
  fun clz(): U128 => @"llvm.ctlz.i128"[U128](this, false)
  fun ctz(): U128 => @"llvm.cttz.i128"[U128](this, false)
  fun bitwidth(): U128 => 128

  fun string(fmt: FormatInt = FormatDefault,
    prefix: PrefixNumber = PrefixDefault, prec: U64 = 1, width: U64 = 0,
    align: Align = AlignRight, fill: U32 = ' '): String iso^
  =>
    _ToString._u128(this, false, fmt, prefix, prec, width, align, fill)

  fun divmod(y: U128): (U128, U128) =>
    ifdef native128 then
      (this / y, this % y)
    else
      if y == 0 then
        return (0, 0)
      end

      var quot: U128 = 0
      var qbit: U128 = 1
      var num = this
      var den = y

      while den.i128() >= 0 do
        den = den << 1
        qbit = qbit << 1
      end

      while qbit != 0 do
        if den <= num then
          num = num - den
          quot = quot + qbit
        end

        den = den >> 1
        qbit = qbit >> 1
      end
      (quot, num)
    end

  fun div(y: U128): U128 =>
    ifdef native128 then
      this / y
    else
      (let q, let r) = divmod(y)
      q
    end

  fun mod(y: U128): U128 =>
    ifdef native128 then
      this % y
    else
      (let q, let r) = divmod(y)
      r
    end

  fun f32(): F32 => this.f64().f32()

  fun f64(): F64 =>
    let low = this.u64()
    let high = (this >> 64).u64()
    let x = low.f64()
    let y = high.f64() * (U128(1) << 64).f64()
    x + y
