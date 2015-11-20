struct Pointer[A]
  """
  A Pointer[A] is a raw memory pointer. It has no descriptor and thus can't be
  included in a union or intersection, or be a subtype of any interface. Most
  functions on a Pointer[A] are private to maintain memory safety.
  """
  new create() =>
    """
    A null pointer.
    """
    compile_intrinsic

  new _alloc(len: U64) =>
    """
    Space for len instances of A.
    """
    compile_intrinsic

  fun ref _realloc(len: U64): Pointer[A] =>
    """
    Keep the contents, but reserve space for len instances of A.
    """
    compile_intrinsic

  fun _apply(i: U64): this->A =>
    """
    Retrieve index i.
    """
    compile_intrinsic

  fun ref _update(i: U64, value: A!): A^ =>
    """
    Set index i and return the previous value.
    """
    compile_intrinsic

  fun _offset(n: U64): this->Pointer[A] =>
    """
    Return a pointer to the n-th element.
    """
    compile_intrinsic

  fun tag _offset_tag(n: U64): Pointer[A] tag =>
    """
    Return a pointer to the n-th element.
    """
    compile_intrinsic

  fun ref _insert(n: U64, len: U64): Pointer[A] =>
    """
    Creates space for n new elements at the head, moving following elements.
    The array length before this should be len, and the available space should
    be at least n + len.
    """
    compile_intrinsic

  fun ref _delete(n: U64, len: U64): A^ =>
    """
    Delete n elements from the head of pointer, compact remaining elements of
    the underlying array. The array length before this should be n + len.
    Returns the first deleted element.
    """
    compile_intrinsic

  fun _copy_to(that: Pointer[this->A!] tag, n: U64): this->Pointer[A] =>
    """
    Copy n elements from this to that.
    """
    compile_intrinsic

  fun tag u64(): U64 =>
    """
    Convert the pointer into a 64 bit integer.
    """
    compile_intrinsic

  fun tag is_null(): Bool =>
    """
    Return true for a null pointer, false for anything else.
    """
    u64() == 0

  fun tag eq(that: Pointer[A] tag): Bool =>
    """
    Return true if this address is that address.
    """
    u64() == that.u64()

  fun tag lt(that: Pointer[A] tag): Bool =>
    """
    Return true if this address is less than that address.
    """
    u64() < that.u64()

  fun tag ne(that: Pointer[A] tag): Bool => not eq(that)
  fun tag le(that: Pointer[A] tag): Bool => lt(that) or eq(that)
  fun tag ge(that: Pointer[A] tag): Bool => not lt(that)
  fun tag gt(that: Pointer[A] tag): Bool => not le(that)

  fun tag hash(): U64 =>
    """
    Returns a hash of the address.
    """
    u64().hash()
