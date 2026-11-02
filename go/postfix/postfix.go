package postfix

import (
	"cmp"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"math"
	"slices"
	"strings"
)

var (
	// ErrUndefinedOperation indicates an unrecognized opcode was encountered.
	ErrUndefinedOperation     = errors.New("undefined operation")
	// ErrIllegalOperation indicates an operation argument or program encoding is malformed.
	ErrIllegalOperation       = errors.New("illegal operation")
	// ErrStackUnderflow indicates the evaluation stack had fewer elements than required.
	ErrStackUnderflow         = errors.New("stack underflow")
	// ErrStackOverflow indicates the required stack capacity exceeds the maximum allowed size.
	ErrStackOverflow          = errors.New("stack overflow")
	// ErrFloatLiteralsUnderflow indicates unexpected EOF when reading embedded float literals.
	ErrFloatLiteralsUnderflow = errors.New("floats underflow")
	// ErrUnexpectedStackSize indicates the final stack length does not match expected output dimensions.
	ErrUnexpectedStackSize    = errors.New("unexpected stack size")
)

// Operation represents a bytecode opcode for the postfix evaluation machine.
type Operation uint8

const (
	Operation_Undefined  Operation = 0
	Operation_Push       Operation = 1
	Operation_Dup        Operation = 2
	Operation_Over       Operation = 3
	Operation_Tuck       Operation = 4
	Operation_Drop       Operation = 5
	Operation_Nip        Operation = 6
	Operation_Swap       Operation = 7
	Operation_Rot        Operation = 8
	Operation_Move       Operation = 9
	Operation_Copy       Operation = 10
	Operation_CopyBase   Operation = 11
	Operation_Slide      Operation = 12
	Operation_Add        Operation = 13
	Operation_Sub        Operation = 14
	Operation_Mul        Operation = 15
	Operation_Div        Operation = 16
	Operation_Pow        Operation = 17
	Operation_Atan2      Operation = 18
	Operation_Neg        Operation = 19
	Operation_Abs        Operation = 20
	Operation_Inv        Operation = 21
	Operation_Sqrt       Operation = 22
	Operation_Exp        Operation = 23
	Operation_Ln         Operation = 24
	Operation_Sin        Operation = 25
	Operation_Cos        Operation = 26
	Operation_Tan        Operation = 27
	Operation_Asin       Operation = 28
	Operation_Acos       Operation = 29
	Operation_Ipow       Operation = 30
	Operation_MulAdd     Operation = 31
	Operation_MulSub     Operation = 32
	Operation_SubMul     Operation = 33
	Operation_PushVec    Operation = 34
	Operation_AddVec     Operation = 35
	Operation_SubVec     Operation = 36
	Operation_MulVec     Operation = 37
	Operation_DivVec     Operation = 38
	Operation_NegVec     Operation = 39
	Operation_InvVec     Operation = 40
	Operation_DotVec     Operation = 41
	Operation_NormSqVec  Operation = 42
	Operation_SumVec     Operation = 43
	Operation_MulAddVec  Operation = 44
	Operation_MulSubVec  Operation = 45
	Operation_SubMulVec  Operation = 46
	Operation_ScaleVec   Operation = 47
	Operation_LerpVec    Operation = 48
	Operation_MulMat     Operation = 49
	Operation_Poly       Operation = 50
	Operation_CosSin     Operation = 51
	Operation_PanTilt    Operation = 52
)

// Expression represents an immutable bytecode sequence of postfix operations.
type Expression struct {
	Prog []uint8
}

// PostfixHeader represents the wire and storage header preceding the bytecode program.
type PostfixHeader struct {
	ProgSize uint16
}

// String returns the human-readable mnemonic of the operation.
func (op Operation) String() string {
	switch op {
	case Operation_Undefined:
		return "Undefined"
	case Operation_Push:
		return "Push"
	case Operation_Dup:
		return "Dup"
	case Operation_Over:
		return "Over"
	case Operation_Tuck:
		return "Tuck"
	case Operation_Drop:
		return "Drop"
	case Operation_Nip:
		return "Nip"
	case Operation_Swap:
		return "Swap"
	case Operation_Rot:
		return "Rot"
	case Operation_Move:
		return "Move"
	case Operation_Copy:
		return "Copy"
	case Operation_CopyBase:
		return "CopyBase"
	case Operation_Slide:
		return "Slide"
	case Operation_Add:
		return "Add"
	case Operation_Sub:
		return "Sub"
	case Operation_Mul:
		return "Mul"
	case Operation_Div:
		return "Div"
	case Operation_Pow:
		return "Pow"
	case Operation_Atan2:
		return "Atan2"
	case Operation_Neg:
		return "Neg"
	case Operation_Abs:
		return "Abs"
	case Operation_Inv:
		return "Inv"
	case Operation_Sqrt:
		return "Sqrt"
	case Operation_Exp:
		return "Exp"
	case Operation_Ln:
		return "Ln"
	case Operation_Sin:
		return "Sin"
	case Operation_Cos:
		return "Cos"
	case Operation_Tan:
		return "Tan"
	case Operation_Asin:
		return "Asin"
	case Operation_Acos:
		return "Acos"
	case Operation_Ipow:
		return "Ipow"
	case Operation_MulAdd:
		return "MulAdd"
	case Operation_MulSub:
		return "MulSub"
	case Operation_SubMul:
		return "SubMul"
	case Operation_PushVec:
		return "PushVec"
	case Operation_AddVec:
		return "AddVec"
	case Operation_SubVec:
		return "SubVec"
	case Operation_MulVec:
		return "MulVec"
	case Operation_DivVec:
		return "DivVec"
	case Operation_NegVec:
		return "NegVec"
	case Operation_InvVec:
		return "InvVec"
	case Operation_DotVec:
		return "DotVec"
	case Operation_NormSqVec:
		return "NormSqVec"
	case Operation_SumVec:
		return "SumVec"
	case Operation_MulAddVec:
		return "MulAddVec"
	case Operation_MulSubVec:
		return "MulSubVec"
	case Operation_SubMulVec:
		return "SubMulVec"
	case Operation_ScaleVec:
		return "ScaleVec"
	case Operation_LerpVec:
		return "LerpVec"
	case Operation_MulMat:
		return "MulMat"
	case Operation_Poly:
		return "Poly"
	case Operation_CosSin:
		return "CosSin"
	case Operation_PanTilt:
		return "PanTilt"
	default:
		return fmt.Sprintf("unknown[%d]", op)
	}
}

// String returns a space-separated representation of the bytecode bytes.
func (expr Expression) String() string {
	var sb strings.Builder
	sb.WriteString("{")
	for i, v := range expr.Prog {
		if i > 0 {
			sb.WriteString(" ")
		}
		sb.WriteString(fmt.Sprint(v))
	}
	sb.WriteString("}")
	return sb.String()
}

// Write serializes the expression with its PostfixHeader to the given writer in little-endian format.
func (expr *Expression) Write(w io.Writer) error {
	header := PostfixHeader{
		ProgSize: uint16(len(expr.Prog)),
	}
	return cmp.Or(
		binary.Write(w, binary.LittleEndian, &header),
		binary.Write(w, binary.LittleEndian, &expr.Prog),
	)
}

// Read deserializes a PostfixHeader and subsequent bytecode bytes from the given reader.
func (expr *Expression) Read(r io.Reader) error {
	var header PostfixHeader
	err := binary.Read(r, binary.LittleEndian, &header)
	if err != nil {
		return err
	}
	expr.Prog = make([]uint8, header.ProgSize)
	return binary.Read(r, binary.LittleEndian, &expr.Prog)
}

// Equal reports whether two expressions have identical bytecode programs.
func Equal(a, b *Expression) bool {
	return slices.Equal(a.Prog, b.Prog)
}

// Join concatenates multiple expressions into a single sequential expression.
func Join(exprs ...*Expression) *Expression {
	result := &Expression{}
	for _, expr := range exprs {
		result.Prog = append(result.Prog, expr.Prog...)
	}
	return result
}

// Builder constructs an Expression by appending bytecode operations and immediate data.
type Builder struct {
	expr Expression
}

// MakeBuilder creates an empty postfix expression builder.
func MakeBuilder() *Builder {
	return &Builder{}
}

// Build returns the completed postfix Expression constructed by this builder.
func (b *Builder) Build() *Expression {
	return &b.expr
}

// Push appends an Operation_Push instruction for a single 32-bit float literal ( -> value).
func (b *Builder) Push(v float64) *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Push))
	b.expr.Prog = binary.LittleEndian.AppendUint32(b.expr.Prog, math.Float32bits(float32(v)))
	return b
}

// PushVec appends an Operation_PushVec instruction with the given 32-bit float vector literals ( -> values...).
func (b *Builder) PushVec(values ...float64) *Builder {
	if len(values) == 0 {
		return b
	}
	if len(values) > 255 {
		panic(fmt.Sprintf("PushVec size (%d) exceeds maximum of 255", len(values)))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_PushVec), uint8(len(values)-1))
	for _, v := range values {
		b.expr.Prog = binary.LittleEndian.AppendUint32(b.expr.Prog, math.Float32bits(float32(v)))
	}
	return b
}

// Drop discards the top value from the stack (a -> ).
func (b *Builder) Drop() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Drop))
	return b
}

// Dup duplicates the top value on the stack (a -> a a).
func (b *Builder) Dup() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Dup))
	return b
}

// Swap exchanges the top two values on the stack (a b -> b a).
func (b *Builder) Swap() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Swap))
	return b
}

// Over copies the second value from the top of the stack onto the top (a b -> a b a).
func (b *Builder) Over() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Over))
	return b
}

// Tuck inserts a copy of the top value beneath the second value (a b -> b a b).
func (b *Builder) Tuck() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Tuck))
	return b
}

// Nip discards the second value from the top of the stack (a b -> b).
func (b *Builder) Nip() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Nip))
	return b
}

// Rot rotates the third value to the top of the stack (a b c -> b c a).
func (b *Builder) Rot() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Rot))
	return b
}

// Copy duplicates a subvector from stack depth offset to the top (... v ... -> ... v ... v).
func (b *Builder) Copy(offset, size int) *Builder {
	if offset < 0 || offset > 31 {
		panic(fmt.Sprintf("Copy offset (%d) must be in range [0, 31]", offset))
	}
	if size < 1 || size > 8 {
		panic(fmt.Sprintf("Copy size (%d) must be in range [1, 8]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Copy), uint8((offset&0x1F)|((size-1)<<5)))
	return b
}

// CopyBase duplicates a subvector starting from index at the bottom of the stack to the top (... v ... -> ... v ... v).
func (b *Builder) CopyBase(index, size int) *Builder {
	if index < 0 || index > 31 {
		panic(fmt.Sprintf("CopyBase index (%d) must be in range [0, 31]", index))
	}
	if size < 1 || size > 8 {
		panic(fmt.Sprintf("CopyBase size (%d) must be in range [1, 8]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_CopyBase), uint8((index&0x1F)|((size-1)<<5)))
	return b
}

// Slide keeps the top keep elements on the stack and drops drop elements directly beneath them.
func (b *Builder) Slide(keep, drop int) *Builder {
	if keep < 1 || keep > 16 {
		panic(fmt.Sprintf("Slide keep (%d) must be in range [1, 16]", keep))
	}
	if drop < 1 || drop > 16 {
		panic(fmt.Sprintf("Slide drop (%d) must be in range [1, 16]", drop))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Slide), uint8(((keep-1)&0x0F)|(((drop-1)&0x0F)<<4)))
	return b
}

// Move rotates a subvector from stack depth offset to the top (... v ... -> ... ... v).
func (b *Builder) Move(offset, size int) *Builder {
	if offset < 0 || offset > 31 {
		panic(fmt.Sprintf("Move offset (%d) must be in range [0, 31]", offset))
	}
	if size < 1 || size > 8 {
		panic(fmt.Sprintf("Move size (%d) must be in range [1, 8]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Move), uint8((offset&0x1F)|((size-1)<<5)))
	return b
}

// Add pops two values and pushes their sum (a b -> a + b).
func (b *Builder) Add() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Add))
	return b
}

// Sub pops rhs and lhs and pushes lhs - rhs (a b -> a - b).
func (b *Builder) Sub() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Sub))
	return b
}

// Mul pops two values and pushes their product (a b -> a * b).
func (b *Builder) Mul() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Mul))
	return b
}

// Div pops rhs and lhs and pushes lhs / rhs (a b -> a / b).
func (b *Builder) Div() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Div))
	return b
}

// Pow pops rhs and lhs and pushes lhs raised to the power of rhs (a b -> a ^ b).
func (b *Builder) Pow() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Pow))
	return b
}

// Atan2 pops y and x and pushes atan2(y, x) (x y -> atan2(y, x)).
func (b *Builder) Atan2() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Atan2))
	return b
}

// Neg negates the top value on the stack (a -> -a).
func (b *Builder) Neg() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Neg))
	return b
}

// Abs replaces the top value on the stack with its absolute value (a -> |a|).
func (b *Builder) Abs() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Abs))
	return b
}

// Inv inverts the top value on the stack (a -> 1 / a).
func (b *Builder) Inv() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Inv))
	return b
}

// Sqrt replaces the top value on the stack with its square root (a -> sqrt(a)).
func (b *Builder) Sqrt() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Sqrt))
	return b
}

// Exp replaces the top value on the stack with e^a (a -> exp(a)).
func (b *Builder) Exp() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Exp))
	return b
}

// Ln replaces the top value on the stack with its natural logarithm ln(a) (a -> ln(a)).
func (b *Builder) Ln() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Ln))
	return b
}

// Sin replaces the top value on the stack with sin(a) (a -> sin(a)).
func (b *Builder) Sin() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Sin))
	return b
}

// Cos replaces the top value on the stack with cos(a) (a -> cos(a)).
func (b *Builder) Cos() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Cos))
	return b
}

// Tan replaces the top value on the stack with tan(a) (a -> tan(a)).
func (b *Builder) Tan() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Tan))
	return b
}

// Asin replaces the top value on the stack with asin(a) (a -> asin(a)).
func (b *Builder) Asin() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Asin))
	return b
}

// Acos replaces the top value on the stack with acos(a) (a -> acos(a)).
func (b *Builder) Acos() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Acos))
	return b
}

// CosSin replaces the top value t on the stack with cos(t) and pushes sin(t) (t -> cos(t) sin(t)).
func (b *Builder) CosSin() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_CosSin))
	return b
}

// PanTilt transforms a 3D line-of-sight vector [v0, v1, v2] on the stack into [pan, tilt] (v[3] -> pan tilt).
func (b *Builder) PanTilt() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_PanTilt))
	return b
}

// Ipow raises the top value on the stack to the integer exponent exp (a -> a ^ exp).
func (b *Builder) Ipow(exp int8) *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Ipow), uint8(exp))
	return b
}

// MulAdd pops c, b, a and pushes a*b + c (a b c -> a * b + c).
func (b *Builder) MulAdd() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_MulAdd))
	return b
}

// MulSub pops c, b, a and pushes a*b - c (a b c -> a * b - c).
func (b *Builder) MulSub() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_MulSub))
	return b
}

// SubMul pops b, a, c and pushes c - a*b (c a b -> c - a * b).
func (b *Builder) SubMul() *Builder {
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_SubMul))
	return b
}

// AddVec pops two vectors of the specified dimension and pushes their componentwise sum (u[N] v[N] -> (u + v)[N]).
func (b *Builder) AddVec(size int) *Builder {
	if size < 1 || size > 255 {
		panic(fmt.Sprintf("AddVec size (%d) must be in range [1, 255]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_AddVec), uint8(size-1))
	return b
}

// SubVec pops vector rhs and vector lhs and pushes their componentwise difference (u[N] v[N] -> (u - v)[N]).
func (b *Builder) SubVec(size int) *Builder {
	if size < 1 || size > 255 {
		panic(fmt.Sprintf("SubVec size (%d) must be in range [1, 255]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_SubVec), uint8(size-1))
	return b
}

// MulVec pops two vectors of the specified dimension and pushes their componentwise product (u[N] v[N] -> (u * v)[N]).
func (b *Builder) MulVec(size int) *Builder {
	if size < 1 || size > 255 {
		panic(fmt.Sprintf("MulVec size (%d) must be in range [1, 255]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_MulVec), uint8(size-1))
	return b
}

// DivVec pops vector rhs and vector lhs and pushes componentwise lhs / rhs (u[N] v[N] -> (u / v)[N]).
func (b *Builder) DivVec(size int) *Builder {
	if size < 1 || size > 255 {
		panic(fmt.Sprintf("DivVec size (%d) must be in range [1, 255]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_DivVec), uint8(size-1))
	return b
}

// NegVec negates each component of a vector on the stack (v[N] -> (-v)[N]).
func (b *Builder) NegVec(size int) *Builder {
	if size < 1 || size > 255 {
		panic(fmt.Sprintf("NegVec size (%d) must be in range [1, 255]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_NegVec), uint8(size-1))
	return b
}

// InvVec inverts each component of a vector on the stack (v[N] -> (1 / v)[N]).
func (b *Builder) InvVec(size int) *Builder {
	if size < 1 || size > 255 {
		panic(fmt.Sprintf("InvVec size (%d) must be in range [1, 255]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_InvVec), uint8(size-1))
	return b
}

// DotVec pops vector rhs and vector lhs and pushes their scalar dot product (u[N] v[N] -> (u · v)).
func (b *Builder) DotVec(size int) *Builder {
	if size < 1 || size > 255 {
		panic(fmt.Sprintf("DotVec size (%d) must be in range [1, 255]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_DotVec), uint8(size-1))
	return b
}

// NormSqVec pops a vector and pushes the scalar squared Euclidean norm ||v||^2 (v[N] -> ||v||^2).
func (b *Builder) NormSqVec(size int) *Builder {
	if size < 1 || size > 255 {
		panic(fmt.Sprintf("NormSqVec size (%d) must be in range [1, 255]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_NormSqVec), uint8(size-1))
	return b
}

// SumVec pops a vector and pushes the scalar sum of its components (v[N] -> sum(v)).
func (b *Builder) SumVec(size int) *Builder {
	if size < 1 || size > 255 {
		panic(fmt.Sprintf("SumVec size (%d) must be in range [1, 255]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_SumVec), uint8(size-1))
	return b
}

// MulAddVec pops vectors c, b, a of the specified dimension and pushes a*b + c componentwise (a[N] b[N] c[N] -> (a * b + c)[N]).
func (b *Builder) MulAddVec(size int) *Builder {
	if size < 1 || size > 255 {
		panic(fmt.Sprintf("MulAddVec size (%d) must be in range [1, 255]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_MulAddVec), uint8(size-1))
	return b
}

// MulSubVec pops vectors c, b, a of the specified dimension and pushes a*b - c componentwise (a[N] b[N] c[N] -> (a * b - c)[N]).
func (b *Builder) MulSubVec(size int) *Builder {
	if size < 1 || size > 255 {
		panic(fmt.Sprintf("MulSubVec size (%d) must be in range [1, 255]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_MulSubVec), uint8(size-1))
	return b
}

// SubMulVec pops vectors b, a, c of the specified dimension and pushes c - a*b componentwise (c[N] a[N] b[N] -> (c - a * b)[N]).
func (b *Builder) SubMulVec(size int) *Builder {
	if size < 1 || size > 255 {
		panic(fmt.Sprintf("SubMulVec size (%d) must be in range [1, 255]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_SubMulVec), uint8(size-1))
	return b
}

// ScaleVec pops vector vec and scalar factor and pushes factor * vec (v[N] s -> (s * v)[N]).
func (b *Builder) ScaleVec(size int) *Builder {
	if size < 1 || size > 255 {
		panic(fmt.Sprintf("ScaleVec size (%d) must be in range [1, 255]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_ScaleVec), uint8(size-1))
	return b
}

// LerpVec pops scalar t and interpolates between immediate vectors v0 and v1 (t -> ((1 - t)*v0 + t*v1)[N]).
func (b *Builder) LerpVec(v0, v1 []float64) *Builder {
	if len(v0) != len(v1) {
		panic(fmt.Sprintf("LerpVec vectors must have same length: len(v0)=%d, len(v1)=%d", len(v0), len(v1)))
	}
	size := len(v0)
	if size < 1 || size > 255 {
		panic(fmt.Sprintf("LerpVec size (%d) must be in range [1, 255]", size))
	}
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_LerpVec), uint8(size-1))
	for i := range size {
		b.expr.Prog = binary.LittleEndian.AppendUint32(b.expr.Prog, math.Float32bits(float32(v0[i])))
		b.expr.Prog = binary.LittleEndian.AppendUint32(b.expr.Prog, math.Float32bits(float32(v1[i])))
	}
	return b
}

func packMulMat(rows, cols int) uint8 {
	if rows < 1 || rows > 4 {
		panic(fmt.Sprintf("MulMat rows (%d) must be in range [1, 4]", rows))
	}
	if cols < 1 || cols > 16 {
		panic(fmt.Sprintf("MulMat cols (%d) must be in range [1, 16]", cols))
	}
	return uint8(((rows - 1) & 0x03) | (((cols - 1) & 0x0F) << 4))
}

func unpackMulMat(arg uint8) (rows, cols int) {
	return int(arg&0x03) + 1, int((arg>>4)&0x0F) + 1
}

// MulMat encodes a vector-matrix multiplication w = v * M with immediate column-major matrix M (v[rows] -> (v * M)[cols]).
// Operand constraints:
//   - rows: 1 to 4 (input vector dimension)
//   - cols: 1 to 16 (output vector dimension)
//   - matrix: elements in column-major order (column by column)
//   - len(matrix) == rows * cols
func (b *Builder) MulMat(rows, cols int, matrix ...float64) *Builder {
	if len(matrix) != rows*cols {
		panic(fmt.Sprintf("invalid number of matrix elements for MulMat: got %d, want %d", len(matrix), rows*cols))
	}
	arg := packMulMat(rows, cols)
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_MulMat), arg)
	for _, f := range matrix {
		b.expr.Prog = binary.LittleEndian.AppendUint32(b.expr.Prog, math.Float32bits(float32(f)))
	}
	return b
}

// Poly evaluates a polynomial or matrix of polynomials for scalar t (t -> P(t)[cols]).
//
// Coefficients must be provided in row-major order with INCREASING degree order:
//   - Row 0 (coeffs[0 : cols]): degree 0 (constant terms)
//   - Row 1 (coeffs[cols : 2*cols]): degree 1 (linear terms)
//   - ...
//   - Row rows-1 (coeffs[(rows-1)*cols : rows*cols]): highest degree (degree = rows - 1)
//
// For dimension j (0 <= j < cols):
//   P_j(t) = coeffs[0*cols + j] + coeffs[1*cols + j]*t + ... + coeffs[(rows-1)*cols + j]*t^(rows-1)
//
// Note: When serializing to bytecode, Poly automatically reverses the row order
// into decreasing degree order (highest degree first) as required by Horner's method
// during bytecode evaluation.
//
// Operand constraints:
//   - rows: 1 to 16 (number of polynomial coefficients per column, i.e. degree + 1)
//   - cols: 1 to 16 (number of dimensions / columns)
//   - len(coeffs) == rows * cols
func (b *Builder) Poly(rows, cols int, coeffs ...float64) *Builder {
	if rows < 1 || rows > 16 || cols < 1 || cols > 16 {
		panic(fmt.Sprintf("Poly rows (%d) and cols (%d) must be in range [1, 16]", rows, cols))
	}
	if len(coeffs) != rows*cols {
		panic("invalid number of coefficients for Poly")
	}
	arg := uint8((rows-1)&0x0F) | (uint8((cols-1)&0x0F) << 4)
	b.expr.Prog = append(b.expr.Prog, uint8(Operation_Poly), arg)
	for i := rows - 1; i >= 0; i-- {
		rowOffset := i * cols
		for j := range cols {
			b.expr.Prog = binary.LittleEndian.AppendUint32(b.expr.Prog, math.Float32bits(float32(coeffs[rowOffset+j])))
		}
	}
	return b
}

// PolyVec is a convenience method that evaluates a 1D polynomial via Operation_Poly (t -> P(t)).
// Coefficients must be provided in INCREASING degree order:
// coeffs[0] is degree 0 (constant), coeffs[1] is degree 1, ..., coeffs[N-1] is degree N-1.
func (b *Builder) PolyVec(coeffs ...float64) *Builder {
	return b.Poly(len(coeffs), 1, coeffs...)
}

// PolyMat is an alias for Poly (t -> P(t)[cols]).
// Coefficients must be provided in row-major order with INCREASING degree order.
func (b *Builder) PolyMat(rows, cols int, coeffs ...float64) *Builder {
	return b.Poly(rows, cols, coeffs...)
}

// Validate performs static analysis of the bytecode against an initial stack length,
// returning the peak stack capacity needed, the final stack length, the opcode trace, or an error.
func (expr *Expression) Validate(initialStackLen int) (minCapacity, finalStackLen int, ops []Operation, err error) {
	pc := 0
	stackLen := initialStackLen
	maxStackLen := initialStackLen

	updateMax := func(depth int) {
		if depth > maxStackLen {
			maxStackLen = depth
		}
	}
	avail := func() int {
		return len(expr.Prog) - pc
	}
	fetchByte := func() int {
		v := expr.Prog[pc]
		pc++
		return int(v)
	}

	for avail() > 0 {
		op := Operation(fetchByte())
		switch op {
		case Operation_Undefined:
			return 0, 0, nil, ErrUndefinedOperation
		case Operation_Push:
			if avail() < 4 {
				return 0, 0, nil, ErrIllegalOperation
			}
			pc += 4
			stackLen += 1
			updateMax(stackLen)
		case Operation_PushVec:
			if avail() < 1 {
				return 0, 0, nil, ErrIllegalOperation
			}
			size := fetchByte() + 1
			byteCount := size * 4
			if avail() < byteCount {
				return 0, 0, nil, ErrIllegalOperation
			}
			pc += byteCount
			stackLen += size
			updateMax(stackLen)
		case Operation_Drop:
			if stackLen < 1 {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen -= 1
		case Operation_Dup:
			if stackLen < 1 {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen += 1
			updateMax(stackLen)
		case Operation_Swap:
			if stackLen < 2 {
				return 0, 0, nil, ErrStackUnderflow
			}
		case Operation_Over:
			if stackLen < 2 {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen += 1
			updateMax(stackLen)
		case Operation_Tuck:
			if stackLen < 2 {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen += 1
			updateMax(stackLen)
		case Operation_Nip:
			if stackLen < 2 {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen -= 1
		case Operation_Rot:
			if stackLen < 3 {
				return 0, 0, nil, ErrStackUnderflow
			}
		case Operation_Move:
			if avail() < 1 {
				return 0, 0, nil, ErrIllegalOperation
			}
			arg := fetchByte()
			offset := arg & 0x1F
			size := (arg >> 5) + 1
			if stackLen < offset+size {
				return 0, 0, nil, ErrStackUnderflow
			}
		case Operation_Copy:
			if avail() < 1 {
				return 0, 0, nil, ErrIllegalOperation
			}
			arg := fetchByte()
			offset := arg & 0x1F
			size := (arg >> 5) + 1
			if stackLen < offset+size {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen += size
			updateMax(stackLen)
		case Operation_CopyBase:
			if avail() < 1 {
				return 0, 0, nil, ErrIllegalOperation
			}
			arg := fetchByte()
			index := int(arg & 0x1F)
			size := int((arg >> 5) + 1)
			if stackLen < index+size {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen += size
			updateMax(stackLen)
		case Operation_Slide:
			if avail() < 1 {
				return 0, 0, nil, ErrIllegalOperation
			}
			arg := fetchByte()
			keep := int(arg&0x0F) + 1
			drop := int(arg>>4) + 1
			if stackLen < keep+drop {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen -= drop
		case Operation_Add, Operation_Sub, Operation_Mul, Operation_Div, Operation_Pow, Operation_Atan2:
			if stackLen < 2 {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen -= 1
		case Operation_Neg, Operation_Abs, Operation_Inv, Operation_Sqrt, Operation_Exp, Operation_Ln, Operation_Sin, Operation_Cos, Operation_Tan, Operation_Asin, Operation_Acos:
			if stackLen < 1 {
				return 0, 0, nil, ErrStackUnderflow
			}
		case Operation_Ipow:
			if avail() < 1 {
				return 0, 0, nil, ErrIllegalOperation
			}
			_ = fetchByte()
			if stackLen < 1 {
				return 0, 0, nil, ErrStackUnderflow
			}
		case Operation_MulAdd, Operation_MulSub, Operation_SubMul:
			if stackLen < 3 {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen -= 2
		case Operation_AddVec, Operation_SubVec, Operation_MulVec, Operation_DivVec:
			if avail() < 1 {
				return 0, 0, nil, ErrIllegalOperation
			}
			size := fetchByte() + 1
			if stackLen < size*2 {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen -= size
		case Operation_NegVec, Operation_InvVec:
			if avail() < 1 {
				return 0, 0, nil, ErrIllegalOperation
			}
			size := fetchByte() + 1
			if stackLen < size {
				return 0, 0, nil, ErrStackUnderflow
			}
		case Operation_DotVec:
			if avail() < 1 {
				return 0, 0, nil, ErrIllegalOperation
			}
			size := fetchByte() + 1
			if stackLen < size*2 {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen -= size*2 - 1
		case Operation_NormSqVec, Operation_SumVec:
			if avail() < 1 {
				return 0, 0, nil, ErrIllegalOperation
			}
			size := fetchByte() + 1
			if stackLen < size {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen = stackLen - size + 1
		case Operation_MulAddVec, Operation_MulSubVec, Operation_SubMulVec:
			if avail() < 1 {
				return 0, 0, nil, ErrIllegalOperation
			}
			size := fetchByte() + 1
			if stackLen < size*3 {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen -= size * 2
		case Operation_ScaleVec:
			if avail() < 1 {
				return 0, 0, nil, ErrIllegalOperation
			}
			size := fetchByte() + 1
			if stackLen < size+1 {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen -= 1
		case Operation_LerpVec:
			if avail() < 1 {
				return 0, 0, nil, ErrIllegalOperation
			}
			size := fetchByte() + 1
			byteCount := size * 2 * 4
			if avail() < byteCount {
				return 0, 0, nil, ErrIllegalOperation
			}
			pc += byteCount
			if stackLen < 1 {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen -= 1
			updateMax(stackLen + size)
			stackLen += size
		case Operation_MulMat:
			if avail() < 1 {
				return 0, 0, nil, ErrIllegalOperation
			}
			arg := fetchByte()
			if (arg & 0x0C) != 0 {
				return 0, 0, nil, ErrIllegalOperation
			}
			rows, cols := unpackMulMat(uint8(arg))
			byteCount := rows * cols * 4
			if avail() < byteCount {
				return 0, 0, nil, ErrIllegalOperation
			}
			pc += byteCount
			if stackLen < rows {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen -= rows
			updateMax(stackLen + cols)
			stackLen += cols
		case Operation_Poly:
			if avail() < 1 {
				return 0, 0, nil, ErrIllegalOperation
			}
			arg := fetchByte()
			rows := int(arg&0x0F) + 1
			cols := int((arg>>4)&0x0F) + 1
			coeffSize := rows * cols
			if avail() < coeffSize*4 {
				return 0, 0, nil, ErrIllegalOperation
			}
			pc += coeffSize * 4
			if stackLen < 1 {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen = stackLen - 1 + cols
			updateMax(stackLen)
		case Operation_CosSin:
			if stackLen < 1 {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen += 1
			updateMax(stackLen)
		case Operation_PanTilt:
			if stackLen < 3 {
				return 0, 0, nil, ErrStackUnderflow
			}
			stackLen -= 1
		default:
			return 0, 0, nil, ErrUndefinedOperation
		}
		ops = append(ops, op)
	}

	if maxStackLen > math.MaxUint16 {
		return 0, 0, nil, ErrStackOverflow
	}

	return maxStackLen, stackLen, ops, nil
}

// Validate performs static analysis of expr against an initial stack length.
func Validate(expr *Expression, initialStackLen int) (minCapacity, finalStackLen int, ops []Operation, err error) {
	return expr.Validate(initialStackLen)
}

func ipow(base float64, exp int) float64 {
	if exp == 0 {
		return 1.0
	}
	if exp < 0 {
		base = 1.0 / base
		exp = -exp
	}
	result := 1.0
	for exp > 0 {
		if exp&1 != 0 {
			result *= base
		}
		base *= base
		exp >>= 1
	}
	return result
}

// Evaluate executes the bytecode on the provided stack without runtime bounds checking.
func (expr *Expression) Evaluate(stack []float64) []float64 {
	pc := 0

	pop := func() float64 {
		i := len(stack) - 1
		v := stack[i]
		stack = stack[:i]
		return v
	}
	popv := func(n int) []float64 {
		i := len(stack) - n
		v := stack[i:]
		stack = stack[:i]
		return v
	}
	fetchByte := func() int {
		v := expr.Prog[pc]
		pc++
		return int(v)
	}
	fetchFloat := func() float64 {
		bits := binary.LittleEndian.Uint32(expr.Prog[pc : pc+4])
		pc += 4
		return float64(math.Float32frombits(bits))
	}

	for pc < len(expr.Prog) {
		op := Operation(fetchByte())
		switch op {
		case Operation_Push:
			stack = append(stack, fetchFloat())
		case Operation_PushVec:
			size := int(fetchByte()) + 1
			for range size {
				stack = append(stack, fetchFloat())
			}
		case Operation_Drop:
			stack = stack[:len(stack)-1]
		case Operation_Dup:
			stack = append(stack, stack[len(stack)-1])
		case Operation_Swap:
			i := len(stack) - 1
			stack[i], stack[i-1] = stack[i-1], stack[i]
		case Operation_Over:
			stack = append(stack, stack[len(stack)-2])
		case Operation_Tuck:
			top := stack[len(stack)-1]
			stack[len(stack)-1] = stack[len(stack)-2]
			stack[len(stack)-2] = top
			stack = append(stack, top)
		case Operation_Nip:
			stack[len(stack)-2] = stack[len(stack)-1]
			stack = stack[:len(stack)-1]
		case Operation_Rot:
			i := len(stack) - 1
			b := stack[i-2]
			stack[i-2] = stack[i-1]
			stack[i-1] = stack[i]
			stack[i] = b
		case Operation_Move:
			arg := fetchByte()
			offset := arg & 0x1F
			size := (arg >> 5) + 1
			if offset > 0 {
				start := len(stack) - offset - size
				mid := start + size
				var tmp [8]float64
				copy(tmp[:size], stack[start:mid])
				copy(stack[start:], stack[mid:])
				copy(stack[len(stack)-size:], tmp[:size])
			}
		case Operation_Copy:
			arg := fetchByte()
			offset := arg & 0x1F
			size := (arg >> 5) + 1
			start := len(stack) - offset - size
			stack = append(stack, stack[start:start+size]...)
		case Operation_CopyBase:
			arg := fetchByte()
			index := int(arg & 0x1F)
			size := int((arg >> 5) + 1)
			stack = append(stack, stack[index:index+size]...)
		case Operation_Slide:
			arg := fetchByte()
			keep := int(arg&0x0F) + 1
			drop := int(arg>>4) + 1
			copy(stack[len(stack)-keep-drop:], stack[len(stack)-keep:])
			stack = stack[:len(stack)-drop]
		case Operation_Add:
			rhs := pop()
			lhs := pop()
			stack = append(stack, lhs+rhs)
		case Operation_Sub:
			rhs := pop()
			lhs := pop()
			stack = append(stack, lhs-rhs)
		case Operation_Mul:
			rhs := pop()
			lhs := pop()
			stack = append(stack, lhs*rhs)
		case Operation_Div:
			rhs := pop()
			lhs := pop()
			stack = append(stack, lhs/rhs)
		case Operation_Pow:
			rhs := pop()
			lhs := pop()
			stack = append(stack, math.Pow(lhs, rhs))
		case Operation_Atan2:
			y := pop()
			x := pop()
			stack = append(stack, math.Atan2(y, x))
		case Operation_Neg:
			v := pop()
			stack = append(stack, -v)
		case Operation_Abs:
			v := pop()
			stack = append(stack, math.Abs(v))
		case Operation_Inv:
			v := pop()
			stack = append(stack, 1/v)
		case Operation_Sqrt:
			v := pop()
			stack = append(stack, math.Sqrt(v))
		case Operation_Exp:
			v := pop()
			stack = append(stack, math.Exp(v))
		case Operation_Ln:
			v := pop()
			stack = append(stack, math.Log(v))
		case Operation_Sin:
			v := pop()
			stack = append(stack, math.Sin(v))
		case Operation_Cos:
			v := pop()
			stack = append(stack, math.Cos(v))
		case Operation_Tan:
			v := pop()
			stack = append(stack, math.Tan(v))
		case Operation_Asin:
			v := pop()
			stack = append(stack, math.Asin(v))
		case Operation_Acos:
			v := pop()
			stack = append(stack, math.Acos(v))
		case Operation_Ipow:
			exp := int8(fetchByte())
			v := pop()
			stack = append(stack, ipow(v, int(exp)))
		case Operation_MulAdd:
			c := pop()
			b := pop()
			a := pop()
			stack = append(stack, a*b+c)
		case Operation_MulSub:
			c := pop()
			b := pop()
			a := pop()
			stack = append(stack, a*b-c)
		case Operation_SubMul:
			b := pop()
			a := pop()
			c := pop()
			stack = append(stack, c-a*b)
		case Operation_AddVec:
			size := fetchByte() + 1
			rhs := popv(size)
			lhs := popv(size)
			for i := range size {
				stack = append(stack, lhs[i]+rhs[i])
			}
		case Operation_SubVec:
			size := fetchByte() + 1
			rhs := popv(size)
			lhs := popv(size)
			for i := range size {
				stack = append(stack, lhs[i]-rhs[i])
			}
		case Operation_MulVec:
			size := fetchByte() + 1
			rhs := popv(size)
			lhs := popv(size)
			for i := range size {
				stack = append(stack, lhs[i]*rhs[i])
			}
		case Operation_DivVec:
			size := fetchByte() + 1
			rhs := popv(size)
			lhs := popv(size)
			for i := range size {
				stack = append(stack, lhs[i]/rhs[i])
			}
		case Operation_NegVec:
			size := fetchByte() + 1
			vec := popv(size)
			for i := range size {
				stack = append(stack, -vec[i])
			}
		case Operation_InvVec:
			size := fetchByte() + 1
			vec := popv(size)
			for i := range size {
				stack = append(stack, 1.0/vec[i])
			}
		case Operation_DotVec:
			size := fetchByte() + 1
			rhs := popv(size)
			lhs := popv(size)
			var result float64
			for i := range size {
				result += lhs[i] * rhs[i]
			}
			stack = append(stack, result)
		case Operation_NormSqVec:
			size := fetchByte() + 1
			vec := popv(size)
			var result float64
			for i := range size {
				result += vec[i] * vec[i]
			}
			stack = append(stack, result)
		case Operation_SumVec:
			size := fetchByte() + 1
			vec := popv(size)
			var result float64
			for i := range size {
				result += vec[i]
			}
			stack = append(stack, result)
		case Operation_MulAddVec:
			size := fetchByte() + 1
			c := popv(size)
			b := popv(size)
			a := popv(size)
			for i := range size {
				stack = append(stack, a[i]*b[i]+c[i])
			}
		case Operation_MulSubVec:
			size := fetchByte() + 1
			c := popv(size)
			b := popv(size)
			a := popv(size)
			for i := range size {
				stack = append(stack, a[i]*b[i]-c[i])
			}
		case Operation_SubMulVec:
			size := fetchByte() + 1
			b := popv(size)
			a := popv(size)
			c := popv(size)
			for i := range size {
				stack = append(stack, c[i]-a[i]*b[i])
			}
		case Operation_ScaleVec:
			size := fetchByte() + 1
			scalar := pop()
			start := len(stack) - size
			for i := start; i < len(stack); i++ {
				stack[i] *= scalar
			}
		case Operation_LerpVec:
			size := int(fetchByte()) + 1
			t := pop()
			for range size {
				v0 := fetchFloat()
				v1 := fetchFloat()
				stack = append(stack, (1-t)*v0+t*v1)
			}
		case Operation_MulMat:
			arg := fetchByte()
			rows, cols := unpackMulMat(uint8(arg))
			v := popv(rows)
			c := make([]float64, cols)
			for j := range cols {
				var r float64
				for i := range rows {
					r += v[i] * fetchFloat()
				}
				c[j] = r
			}
			stack = append(stack, c...)
		case Operation_Poly:
			arg := fetchByte()
			rows := int(arg&0x0F) + 1
			cols := int((arg>>4)&0x0F) + 1
			t := pop()
			if cols == 1 {
				result := fetchFloat()
				for range rows - 1 {
					result = result*t + fetchFloat()
				}
				stack = append(stack, result)
			} else {
				result := make([]float64, cols)
				for j := range cols {
					result[j] = fetchFloat()
				}
				for range rows - 1 {
					for j := range cols {
						result[j] = result[j]*t + fetchFloat()
					}
				}
				stack = append(stack, result...)
			}
		case Operation_CosSin:
			t := pop()
			s, c := math.Sincos(t)
			stack = append(stack, c, s)
		case Operation_PanTilt:
			v2 := pop()
			v1 := pop()
			v0 := pop()
			r := math.Sqrt(v0*v0 + v1*v1)
			stack = append(stack, math.Atan2(v1, v0), math.Atan2(v2, r))
		}
	}
	return stack
}

// Evaluate executes expr on stack without runtime bounds checking.
func Evaluate(expr *Expression, stack []float64) []float64 {
	return expr.Evaluate(stack)
}

// Eval validates the expression and executes it on the provided stack, returning an error if validation fails.
func (expr *Expression) Eval(stack []float64) ([]float64, error) {
	if _, _, _, err := expr.Validate(len(stack)); err != nil {
		return nil, err
	}
	return expr.Evaluate(stack), nil
}

// Eval validates expr and executes it on stack, returning an error if validation fails.
func Eval(expr *Expression, stack []float64) ([]float64, error) {
	return expr.Eval(stack)
}
