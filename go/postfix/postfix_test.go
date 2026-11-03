package postfix_test

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"math"
	"testing"

	"github.com/ScottDuckworth/WickedWinchProtocol/go/postfix"
)

func TestPostfixHeaderSize(t *testing.T) {
	var header postfix.PostfixHeader
	var buf bytes.Buffer
	binary.Write(&buf, binary.LittleEndian, &header)
	if got, want := buf.Len(), 2; got != want {
		t.Errorf("PostfixHeader size: got %v, want %v", got, want)
	}
}

func equalSlices(got, want []float64) bool {
	if len(got) != len(want) {
		return false
	}
	for i := range got {
		if math.Abs(got[i]-want[i]) > 0.001 {
			return false
		}
	}
	return true
}

func TestJoin(t *testing.T) {
	for _, test := range []struct {
		name  string
		exprs []*postfix.Expression
		want  *postfix.Expression
	}{
		{
			name:  "empty",
			exprs: nil,
			want:  &postfix.Expression{},
		},
		{
			name: "one",
			exprs: []*postfix.Expression{
				postfix.MakeBuilder().Push(10).Build(),
			},
			want: postfix.MakeBuilder().Push(10).Build(),
		},
		{
			name: "two",
			exprs: []*postfix.Expression{
				postfix.MakeBuilder().Push(10).Build(),
				postfix.MakeBuilder().Drop().Build(),
			},
			want: postfix.MakeBuilder().Push(10).Drop().Build(),
		},
	} {
		t.Run(test.name, func(t *testing.T) {
			got := postfix.Join(test.exprs...)
			if !postfix.Equal(got, test.want) {
				t.Errorf("got %v, want %v", got, test.want)
			}
		})
	}
}

func TestEvalPostfixExpression(t *testing.T) {
	for _, test := range []struct {
		name      string
		expr      *postfix.Expression
		stack     []float64
		wantStack []float64
		wantErr   error
	}{
		{
			name: "undefined",
			expr: &postfix.Expression{
				Prog: []uint8{uint8(postfix.Operation_Undefined)},
			},
			wantErr: postfix.ErrUndefinedOperation,
		},
		{
			name:      "push 0",
			expr:      postfix.MakeBuilder().PushVec().Build(),
			stack:     []float64{0},
			wantStack: []float64{0},
		},
		{
			name:      "push 1",
			expr:      postfix.MakeBuilder().Push(1).Build(),
			stack:     []float64{0},
			wantStack: []float64{0, 1},
		},
		{
			name:      "push 2",
			expr:      postfix.MakeBuilder().PushVec(1, 2).Build(),
			stack:     []float64{0},
			wantStack: []float64{0, 1, 2},
		},
		{
			name: "push float literal underflow",
			expr: &postfix.Expression{
				Prog: []uint8{uint8(postfix.Operation_Push), 1, 2},
			},
			wantErr: postfix.ErrIllegalOperation,
		},
		{
			name: "pushvec count underflow",
			expr: &postfix.Expression{
				Prog: []uint8{uint8(postfix.Operation_PushVec)},
			},
			wantErr: postfix.ErrIllegalOperation,
		},
		{
			name: "pushvec float literals underflow",
			expr: &postfix.Expression{
				Prog: []uint8{uint8(postfix.Operation_PushVec), 0, 1, 2},
			},
			wantErr: postfix.ErrIllegalOperation,
		},
		{
			name:      "drop",
			expr:      postfix.MakeBuilder().Drop().Build(),
			stack:     []float64{0, 1, 2},
			wantStack: []float64{0, 1},
		},
		{
			name:    "drop stack underflow",
			expr:    postfix.MakeBuilder().Drop().Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "dup",
			expr:      postfix.MakeBuilder().Dup().Build(),
			stack:     []float64{0, 1, 2},
			wantStack: []float64{0, 1, 2, 2},
		},
		{
			name:    "dup stack underflow",
			expr:    postfix.MakeBuilder().Dup().Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "swap",
			expr:      postfix.MakeBuilder().Swap().Build(),
			stack:     []float64{0, 1, 2},
			wantStack: []float64{0, 2, 1},
		},
		{
			name:    "swap stack underflow",
			expr:    postfix.MakeBuilder().Swap().Build(),
			stack:   []float64{0},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "over",
			expr:      postfix.MakeBuilder().Over().Build(),
			stack:     []float64{0, 1},
			wantStack: []float64{0, 1, 0},
		},
		{
			name:    "over stack underflow",
			expr:    postfix.MakeBuilder().Over().Build(),
			stack:   []float64{0},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "tuck",
			expr:      postfix.MakeBuilder().Tuck().Build(),
			stack:     []float64{0, 1},
			wantStack: []float64{1, 0, 1},
		},
		{
			name:    "tuck stack underflow",
			expr:    postfix.MakeBuilder().Tuck().Build(),
			stack:   []float64{0},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "nip",
			expr:      postfix.MakeBuilder().Nip().Build(),
			stack:     []float64{0, 1, 2},
			wantStack: []float64{0, 2},
		},
		{
			name:    "nip stack underflow",
			expr:    postfix.MakeBuilder().Nip().Build(),
			stack:   []float64{0},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "rot",
			expr:      postfix.MakeBuilder().Rot().Build(),
			stack:     []float64{0, 1, 2},
			wantStack: []float64{1, 2, 0},
		},
		{
			name:    "rot stack underflow",
			expr:    postfix.MakeBuilder().Rot().Build(),
			stack:   []float64{0, 1},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "copy scalar",
			expr:      postfix.MakeBuilder().Copy(1, 1).Build(),
			stack:     []float64{10, 20, 30},
			wantStack: []float64{10, 20, 30, 20},
		},
		{
			name:      "copy vector",
			expr:      postfix.MakeBuilder().Copy(1, 2).Build(),
			stack:     []float64{10, 20, 30, 40},
			wantStack: []float64{10, 20, 30, 40, 20, 30},
		},
		{
			name:      "copy max size",
			expr:      postfix.MakeBuilder().Copy(0, 8).Build(),
			stack:     []float64{1, 2, 3, 4, 5, 6, 7, 8},
			wantStack: []float64{1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8},
		},
		{
			name:      "move scalar",
			expr:      postfix.MakeBuilder().Move(2, 1).Build(),
			stack:     []float64{10, 20, 30, 40},
			wantStack: []float64{10, 30, 40, 20},
		},
		{
			name:      "move vector",
			expr:      postfix.MakeBuilder().Move(1, 2).Build(),
			stack:     []float64{10, 20, 30, 40},
			wantStack: []float64{10, 40, 20, 30},
		},
		{
			name:      "move max size",
			expr:      postfix.MakeBuilder().Move(1, 8).Build(),
			stack:     []float64{1, 2, 3, 4, 5, 6, 7, 8, 9},
			wantStack: []float64{9, 1, 2, 3, 4, 5, 6, 7, 8},
		},
		{
			name:    "copy stack underflow",
			expr:    postfix.MakeBuilder().Copy(2, 2).Build(),
			stack:   []float64{10, 20, 30},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name: "copy int literals underflow",
			expr: &postfix.Expression{
				Prog: []uint8{uint8(postfix.Operation_Copy)},
			},
			wantErr: postfix.ErrIllegalOperation,
		},
		{
			name:    "move stack underflow",
			expr:    postfix.MakeBuilder().Move(2, 2).Build(),
			stack:   []float64{10, 20, 30},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name: "move int literals underflow",
			expr: &postfix.Expression{
				Prog: []uint8{uint8(postfix.Operation_Move)},
			},
			wantErr: postfix.ErrIllegalOperation,
		},
		{
			name:      "copybase scalar",
			expr:      postfix.MakeBuilder().CopyBase(0, 1).Build(),
			stack:     []float64{10, 20, 30},
			wantStack: []float64{10, 20, 30, 10},
		},
		{
			name:      "copybase vector",
			expr:      postfix.MakeBuilder().CopyBase(1, 2).Build(),
			stack:     []float64{10, 20, 30, 40},
			wantStack: []float64{10, 20, 30, 40, 20, 30},
		},
		{
			name:      "copybase max size",
			expr:      postfix.MakeBuilder().CopyBase(0, 8).Build(),
			stack:     []float64{1, 2, 3, 4, 5, 6, 7, 8},
			wantStack: []float64{1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4, 5, 6, 7, 8},
		},
		{
			name:    "copybase stack underflow",
			expr:    postfix.MakeBuilder().CopyBase(2, 2).Build(),
			stack:   []float64{10, 20, 30},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name: "copybase int literals underflow",
			expr: &postfix.Expression{
				Prog: []uint8{uint8(postfix.Operation_CopyBase)},
			},
			wantErr: postfix.ErrIllegalOperation,
		},
		{
			name:      "slide",
			expr:      postfix.MakeBuilder().Slide(2, 3).Build(),
			stack:     []float64{10, 20, 30, 40, 50},
			wantStack: []float64{40, 50},
		},
		{
			name:      "slide scalar",
			expr:      postfix.MakeBuilder().Slide(1, 1).Build(),
			stack:     []float64{10, 20},
			wantStack: []float64{20},
		},
		{
			name:    "slide underflow",
			expr:    postfix.MakeBuilder().Slide(2, 2).Build(),
			stack:   []float64{10, 20, 30},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name: "slide int literals underflow",
			expr: &postfix.Expression{
				Prog: []uint8{uint8(postfix.Operation_Slide)},
			},
			wantErr: postfix.ErrIllegalOperation,
		},
		{
			name:      "add",
			expr:      postfix.MakeBuilder().Add().Build(),
			stack:     []float64{0, 1, 2},
			wantStack: []float64{0, 3},
		},
		{
			name:    "add underflow",
			expr:    postfix.MakeBuilder().Add().Build(),
			stack:   []float64{1},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "subtract",
			expr:      postfix.MakeBuilder().Sub().Build(),
			stack:     []float64{0, 1, 2},
			wantStack: []float64{0, -1},
		},
		{
			name:    "subtract underflow",
			expr:    postfix.MakeBuilder().Sub().Build(),
			stack:   []float64{1},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "multiply",
			expr:      postfix.MakeBuilder().Mul().Build(),
			stack:     []float64{0, 10, 2},
			wantStack: []float64{0, 20},
		},
		{
			name:    "multiply underflow",
			expr:    postfix.MakeBuilder().Mul().Build(),
			stack:   []float64{1},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "divide",
			expr:      postfix.MakeBuilder().Div().Build(),
			stack:     []float64{0, 10, 2},
			wantStack: []float64{0, 5},
		},
		{
			name:    "divide underflow",
			expr:    postfix.MakeBuilder().Div().Build(),
			stack:   []float64{1},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "power",
			expr:      postfix.MakeBuilder().Pow().Build(),
			stack:     []float64{0, 10, 2},
			wantStack: []float64{0, 100},
		},
		{
			name:    "power underflow",
			expr:    postfix.MakeBuilder().Pow().Build(),
			stack:   []float64{1},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "atan2",
			expr:      postfix.MakeBuilder().Atan2().Build(),
			stack:     []float64{0, 1},
			wantStack: []float64{math.Pi / 2},
		},
		{
			name:    "atan2 underflow",
			expr:    postfix.MakeBuilder().Atan2().Build(),
			stack:   []float64{1},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "negate",
			expr:      postfix.MakeBuilder().Neg().Build(),
			stack:     []float64{0, 1},
			wantStack: []float64{0, -1},
		},
		{
			name:    "negate underflow",
			expr:    postfix.MakeBuilder().Neg().Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "abs positive",
			expr:      postfix.MakeBuilder().Abs().Build(),
			stack:     []float64{0, 1},
			wantStack: []float64{0, 1},
		},
		{
			name:      "abs negative",
			expr:      postfix.MakeBuilder().Abs().Build(),
			stack:     []float64{0, -1},
			wantStack: []float64{0, 1},
		},
		{
			name:    "abs underflow",
			expr:    postfix.MakeBuilder().Abs().Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "inverse",
			expr:      postfix.MakeBuilder().Inv().Build(),
			stack:     []float64{0, 2},
			wantStack: []float64{0, 0.5},
		},
		{
			name:    "inverse underflow",
			expr:    postfix.MakeBuilder().Inv().Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "sqrt",
			expr:      postfix.MakeBuilder().Sqrt().Build(),
			stack:     []float64{0, 4},
			wantStack: []float64{0, 2},
		},
		{
			name:    "sqrt underflow",
			expr:    postfix.MakeBuilder().Sqrt().Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "exp",
			expr:      postfix.MakeBuilder().Exp().Build(),
			stack:     []float64{0, math.Log(2)},
			wantStack: []float64{0, 2},
		},
		{
			name:    "exp underflow",
			expr:    postfix.MakeBuilder().Exp().Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "log",
			expr:      postfix.MakeBuilder().Ln().Build(),
			stack:     []float64{0, math.Exp(2)},
			wantStack: []float64{0, 2},
		},
		{
			name:    "log underflow",
			expr:    postfix.MakeBuilder().Ln().Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "sin",
			expr:      postfix.MakeBuilder().Sin().Build(),
			stack:     []float64{0, math.Pi / 2},
			wantStack: []float64{0, 1},
		},
		{
			name:    "sin underflow",
			expr:    postfix.MakeBuilder().Sin().Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "cos",
			expr:      postfix.MakeBuilder().Cos().Build(),
			stack:     []float64{0, math.Pi},
			wantStack: []float64{0, -1},
		},
		{
			name:    "cos underflow",
			expr:    postfix.MakeBuilder().Cos().Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "tan",
			expr:      postfix.MakeBuilder().Tan().Build(),
			stack:     []float64{0, math.Pi / 4},
			wantStack: []float64{0, 1},
		},
		{
			name:    "tan underflow",
			expr:    postfix.MakeBuilder().Tan().Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "asin",
			expr:      postfix.MakeBuilder().Asin().Build(),
			stack:     []float64{0, 0.5},
			wantStack: []float64{0, math.Pi / 6},
		},
		{
			name:    "asin underflow",
			expr:    postfix.MakeBuilder().Asin().Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "acos",
			expr:      postfix.MakeBuilder().Acos().Build(),
			stack:     []float64{0, 0.5},
			wantStack: []float64{0, math.Pi * 2 / 6},
		},
		{
			name:    "acos underflow",
			expr:    postfix.MakeBuilder().Acos().Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "cossin 0",
			expr:      postfix.MakeBuilder().CosSin().Build(),
			stack:     []float64{0},
			wantStack: []float64{1, 0},
		},
		{
			name:      "cossin pi/2",
			expr:      postfix.MakeBuilder().CosSin().Build(),
			stack:     []float64{math.Pi / 2},
			wantStack: []float64{0, 1},
		},
		{
			name:    "cossin underflow",
			expr:    postfix.MakeBuilder().CosSin().Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "pantilt",
			expr:      postfix.MakeBuilder().PanTilt().Build(),
			stack:     []float64{0, 3, 4, 5},
			wantStack: []float64{0, math.Atan2(4, 3), math.Atan2(5, 5)},
		},
		{
			name:    "pantilt underflow",
			expr:    postfix.MakeBuilder().PanTilt().Build(),
			stack:   []float64{1, 2},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "ipow positive",
			expr:      postfix.MakeBuilder().Ipow(3).Build(),
			stack:     []float64{0, 2},
			wantStack: []float64{0, 8},
		},
		{
			name:      "ipow zero",
			expr:      postfix.MakeBuilder().Ipow(0).Build(),
			stack:     []float64{0, 5},
			wantStack: []float64{0, 1},
		},
		{
			name:      "ipow negative",
			expr:      postfix.MakeBuilder().Ipow(-2).Build(),
			stack:     []float64{0, 2},
			wantStack: []float64{0, 0.25},
		},
		{
			name:    "ipow underflow",
			expr:    postfix.MakeBuilder().Ipow(2).Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name: "ipow truncated",
			expr: &postfix.Expression{
				Prog: []uint8{uint8(postfix.Operation_Ipow)},
			},
			stack:   []float64{2},
			wantErr: postfix.ErrIllegalOperation,
		},
		{
			name:      "multiply add",
			expr:      postfix.MakeBuilder().MulAdd().Build(),
			stack:     []float64{10, 2, 1},
			wantStack: []float64{21},
		},
		{
			name:    "multiply add underflow",
			expr:    postfix.MakeBuilder().MulAdd().Build(),
			stack:   []float64{1, 2},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "multiply subtract",
			expr:      postfix.MakeBuilder().MulSub().Build(),
			stack:     []float64{10, 2, 1},
			wantStack: []float64{19},
		},
		{
			name:    "multiply subtract underflow",
			expr:    postfix.MakeBuilder().MulSub().Build(),
			stack:   []float64{1, 2},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "subtract multiply",
			expr:      postfix.MakeBuilder().SubMul().Build(),
			stack:     []float64{25, 10, 2},
			wantStack: []float64{5},
		},
		{
			name:    "subtract multiply underflow",
			expr:    postfix.MakeBuilder().SubMul().Build(),
			stack:   []float64{1, 2},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "add vector 3",
			expr:      postfix.MakeBuilder().AddVec(3).Build(),
			stack:     []float64{1, 2, 3, 4, 5, 6},
			wantStack: []float64{1 + 4, 2 + 5, 3 + 6},
		},
		{
			name:    "add vector underflow",
			expr:    postfix.MakeBuilder().AddVec(3).Build(),
			stack:   []float64{1, 2, 3, 4, 5},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "subtract vector 3",
			expr:      postfix.MakeBuilder().SubVec(3).Build(),
			stack:     []float64{1, 2, 3, 4, 5, 6},
			wantStack: []float64{1 - 4, 2 - 5, 3 - 6},
		},
		{
			name:    "subtract vector underflow",
			expr:    postfix.MakeBuilder().SubVec(3).Build(),
			stack:   []float64{1, 2, 3, 4, 5},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "multiply vector 3",
			expr:      postfix.MakeBuilder().MulVec(3).Build(),
			stack:     []float64{1, 2, 3, 4, 5, 6},
			wantStack: []float64{1 * 4, 2 * 5, 3 * 6},
		},
		{
			name:    "multiply vector underflow",
			expr:    postfix.MakeBuilder().MulVec(3).Build(),
			stack:   []float64{1, 2, 3, 4, 5},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "divide vector 3",
			expr:      postfix.MakeBuilder().DivVec(3).Build(),
			stack:     []float64{6, 8, -10, 2, 4, 5},
			wantStack: []float64{3, 2, -2},
		},
		{
			name:    "divide vector underflow",
			expr:    postfix.MakeBuilder().DivVec(3).Build(),
			stack:   []float64{6, 8, -10, 2, 4},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "negate vector 3",
			expr:      postfix.MakeBuilder().NegVec(3).Build(),
			stack:     []float64{1, 2, -3},
			wantStack: []float64{-1, -2, 3},
		},
		{
			name:    "negate vector underflow",
			expr:    postfix.MakeBuilder().NegVec(3).Build(),
			stack:   []float64{1, 2},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "abs vector 3",
			expr:      postfix.MakeBuilder().AbsVec(3).Build(),
			stack:     []float64{1, -2, -3.5},
			wantStack: []float64{1, 2, 3.5},
		},
		{
			name:    "abs vector underflow",
			expr:    postfix.MakeBuilder().AbsVec(3).Build(),
			stack:   []float64{1, 2},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "invert vector 3",
			expr:      postfix.MakeBuilder().InvVec(3).Build(),
			stack:     []float64{2, 4, -0.5},
			wantStack: []float64{0.5, 0.25, -2},
		},
		{
			name:    "invert vector underflow",
			expr:    postfix.MakeBuilder().InvVec(3).Build(),
			stack:   []float64{2, 4},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "dot vector 3",
			expr:      postfix.MakeBuilder().DotVec(3).Build(),
			stack:     []float64{0, 1, 2, 3, 4, 5, 6},
			wantStack: []float64{0, 32},
		},
		{
			name:    "dot vector underflow",
			expr:    postfix.MakeBuilder().DotVec(3).Build(),
			stack:   []float64{0, 1, 2, 3, 4},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "norm squared vector 2",
			expr:      postfix.MakeBuilder().NormSqVec(2).Build(),
			stack:     []float64{3, 4},
			wantStack: []float64{25},
		},
		{
			name:      "norm squared and sqrt vector 2",
			expr:      postfix.MakeBuilder().NormSqVec(2).Sqrt().Build(),
			stack:     []float64{3, 4},
			wantStack: []float64{5},
		},
		{
			name:    "norm squared vector underflow",
			expr:    postfix.MakeBuilder().NormSqVec(2).Build(),
			stack:   []float64{3},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "sum vector 3",
			expr:      postfix.MakeBuilder().SumVec(3).Build(),
			stack:     []float64{1, 2.5, -0.5},
			wantStack: []float64{3.0},
		},
		{
			name:    "sum vector underflow",
			expr:    postfix.MakeBuilder().SumVec(3).Build(),
			stack:   []float64{1, 2},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "multiply add vector 3",
			expr:      postfix.MakeBuilder().MulAddVec(3).Build(),
			stack:     []float64{1, 2, 3, 4, 5, 6, 7, 8, 9},
			wantStack: []float64{1*4 + 7, 2*5 + 8, 3*6 + 9},
		},
		{
			name:    "multiply add vector underflow",
			expr:    postfix.MakeBuilder().MulAddVec(3).Build(),
			stack:   []float64{1, 2, 3, 4, 5, 6, 7, 8},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "push 1 multiply add vector 3 implicit",
			expr:      postfix.MakeBuilder().PushVec(7, 8, 9).MulAddVec(3).Build(),
			stack:     []float64{1, 2, 3, 4, 5, 6},
			wantStack: []float64{1*4 + 7, 2*5 + 8, 3*6 + 9},
		},
		{
			name:      "push 2 multiply add vector 3 implicit",
			expr:      postfix.MakeBuilder().PushVec(4, 5, 6, 7, 8, 9).MulAddVec(3).Build(),
			stack:     []float64{1, 2, 3},
			wantStack: []float64{1*4 + 7, 2*5 + 8, 3*6 + 9},
		},
		{
			name:      "multiply subtract vector 3",
			expr:      postfix.MakeBuilder().MulSubVec(3).Build(),
			stack:     []float64{1, 2, 3, 4, 5, 6, 7, 8, 9},
			wantStack: []float64{1*4 - 7, 2*5 - 8, 3*6 - 9},
		},
		{
			name:    "multiply subtract vector underflow",
			expr:    postfix.MakeBuilder().MulSubVec(3).Build(),
			stack:   []float64{1, 2, 3, 4, 5, 6, 7, 8},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "subtract multiply vector 3",
			expr:      postfix.MakeBuilder().SubMulVec(3).Build(),
			stack:     []float64{10, 20, 30, 1, 2, 3, 4, 5, 6},
			wantStack: []float64{10 - 1*4, 20 - 2*5, 30 - 3*6},
		},
		{
			name:    "subtract multiply vector underflow",
			expr:    postfix.MakeBuilder().SubMulVec(3).Build(),
			stack:   []float64{1, 2, 3, 4, 5, 6, 7, 8},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "scale vector 3",
			expr:      postfix.MakeBuilder().ScaleVec(3).Build(),
			stack:     []float64{1, 2, 3, 2},
			wantStack: []float64{2, 4, 6},
		},
		{
			name:    "scale vector underflow",
			expr:    postfix.MakeBuilder().ScaleVec(3).Build(),
			stack:   []float64{1, 2, 3},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:  "multiply matrix 3 4",
			expr:  postfix.MakeBuilder().MulMat(3, 4, 1, 5, 9, 2, 6, 10, 3, 7, 11, 4, 8, 12).Build(),
			stack: []float64{1, 2, 3},
			wantStack: []float64{
				1*1 + 2*5 + 3*9, 1*2 + 2*6 + 3*10, 1*3 + 2*7 + 3*11, 1*4 + 2*8 + 3*12,
			},
		},
		{
			name:      "multiply matrix square 3 3",
			expr:      postfix.MakeBuilder().MulMat(3, 3, 1, 0, 0, 0, 1, 0, 0, 0, 1).Build(),
			stack:     []float64{1, 2, 3},
			wantStack: []float64{1, 2, 3},
		},
		{
			name:      "multiply matrix projection down 3 2",
			expr:      postfix.MakeBuilder().MulMat(3, 2, 1, 3, 5, 2, 4, 6).Build(),
			stack:     []float64{1, 2, 3},
			wantStack: []float64{1*1 + 2*3 + 3*5, 1*2 + 2*4 + 3*6},
		},
		{
			name:      "multiply matrix dot product 3 1",
			expr:      postfix.MakeBuilder().MulMat(3, 1, 4, 5, 6).Build(),
			stack:     []float64{1, 2, 3},
			wantStack: []float64{1*4 + 2*5 + 3*6},
		},
		{
			name:      "multiply matrix arbitrary projection 4 2",
			expr:      postfix.MakeBuilder().MulMat(4, 2, 1, 3, 5, 7, 2, 4, 6, 8).Build(),
			stack:     []float64{1, 1, 1, 1},
			wantStack: []float64{1 + 3 + 5 + 7, 2 + 4 + 6 + 8},
		},
		{
			name:    "multiply matrix underflow",
			expr:    postfix.MakeBuilder().MulMat(3, 4, 1, 5, 9, 2, 6, 10, 3, 7, 11, 4, 8, 12).Build(),
			stack:   []float64{1, 2},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:    "multiply matrix reserved bits non zero",
			expr:    &postfix.Expression{Prog: []uint8{uint8(postfix.Operation_MulMat), 0x04, 0, 0, 0, 0}},
			stack:   []float64{1},
			wantErr: postfix.ErrIllegalOperation,
		},
		{
			name:      "poly vector 1",
			expr:      postfix.MakeBuilder().Poly(1, 1, 1).Build(),
			stack:     []float64{2},
			wantStack: []float64{1},
		},
		{
			name:      "poly vector 2",
			expr:      postfix.MakeBuilder().Poly(2, 1, 1, 3).Build(),
			stack:     []float64{2},
			wantStack: []float64{7},
		},
		{
			name:      "poly vector 3",
			expr:      postfix.MakeBuilder().Poly(3, 1, 1, 3, 5).Build(),
			stack:     []float64{2},
			wantStack: []float64{27},
		},
		{
			name:    "poly vector underflow",
			expr:    postfix.MakeBuilder().Poly(1, 1, 1).Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
		{
			name:      "poly matrix 3x2",
			expr:      postfix.MakeBuilder().Poly(3, 2, 1, 2, 3, 4, 5, 6).Build(),
			stack:     []float64{2},
			wantStack: []float64{1 + 2*3 + 2*2*5, 2 + 2*4 + 2*2*6},
		},
		{
			name:    "poly matrix underflow",
			expr:    postfix.MakeBuilder().Poly(3, 2, 1, 2, 3, 4, 5, 6).Build(),
			stack:   []float64{},
			wantErr: postfix.ErrStackUnderflow,
		},
	} {
		t.Run(test.name, func(t *testing.T) {
			gotStack, gotErr := postfix.Eval(test.expr, test.stack)
			if !equalSlices(gotStack, test.wantStack) {
				t.Errorf("got %v, want %v", gotStack, test.wantStack)
			}
			if gotErr != test.wantErr {
				t.Errorf("got %v, want %v", gotErr, test.wantErr)
			}

			var buf bytes.Buffer
			if err := test.expr.Write(&buf); err != nil {
				t.Errorf("write error: %v", err)
			}
			var expr postfix.Expression
			if err := expr.Read(&buf); err != nil {
				t.Errorf("read error: %v", err)
			}
			if got, want := &expr, test.expr; !postfix.Equal(got, want) {
				t.Errorf("after serialization: got %v, want %v", got, want)
			}
		})
	}
}
func TestValidateAndEvaluate(t *testing.T) {
	// Push 1, 2 to an initial stack of [42]. Initial length = 1.
	// Expected final stack is [42, 1, 2], final length = 3, minCapacity = 3.
	expr := postfix.MakeBuilder().PushVec(1, 2).Build()

	minCapacity, finalStackLen, _, err := postfix.Validate(expr, 1)
	if err != nil {
		t.Fatalf("unexpected validation error: %v", err)
	}
	if minCapacity != 3 {
		t.Errorf("got minCapacity %d, want 3", minCapacity)
	}
	if finalStackLen != 3 {
		t.Errorf("got finalStackLen %d, want 3", finalStackLen)
	}

	minCapExpr, finalLenExpr, _, err := expr.Validate(1)
	if err != nil {
		t.Fatalf("unexpected validation error: %v", err)
	}
	if minCapExpr != 3 || finalLenExpr != 3 {
		t.Errorf("got minCapacity %d, finalStackLen %d; want 3, 3", minCapExpr, finalLenExpr)
	}

	// Caller checking expected final stack length
	checkExpectedFinalLen := func(expected int) error {
		_, finalLen, _, err := expr.Validate(1)
		if err != nil {
			return err
		}
		if finalLen != expected {
			return postfix.ErrUnexpectedStackSize
		}
		return nil
	}

	// Validation with mismatched expected final stack length
	if err := checkExpectedFinalLen(2); !errors.Is(err, postfix.ErrUnexpectedStackSize) {
		t.Errorf("got %v, want %v", err, postfix.ErrUnexpectedStackSize)
	}
	if err := checkExpectedFinalLen(4); !errors.Is(err, postfix.ErrUnexpectedStackSize) {
		t.Errorf("got %v, want %v", err, postfix.ErrUnexpectedStackSize)
	}
	if err := checkExpectedFinalLen(3); err != nil {
		t.Errorf("unexpected error: %v", err)
	}

	// Direct evaluation
	got := expr.Evaluate([]float64{42})
	want := []float64{42, 1, 2}
	if !equalSlices(got, want) {
		t.Errorf("got %v, want %v", got, want)
	}

	// Package-level Evaluate
	gotPkg := postfix.Evaluate(expr, []float64{42})
	if !equalSlices(gotPkg, want) {
		t.Errorf("got %v, want %v", gotPkg, want)
	}
}

func TestValidateStackOverflow(t *testing.T) {
	expr := postfix.MakeBuilder().Dup().Build()
	_, _, _, err := expr.Validate(math.MaxUint16)
	if !errors.Is(err, postfix.ErrStackOverflow) {
		t.Errorf("got %v, want %v", err, postfix.ErrStackOverflow)
	}
}

func TestVectorBuilderSizeValidation(t *testing.T) {
	assertPanics := func(name string, f func()) {
		t.Helper()
		defer func() {
			if r := recover(); r == nil {
				t.Errorf("%s: expected panic, but did not panic", name)
			}
		}()
		f()
	}

	for _, invalidSize := range []int{0, -1, 256} {
		assertPanics(fmt.Sprintf("AddVec(%d)", invalidSize), func() { postfix.MakeBuilder().AddVec(invalidSize) })
		assertPanics(fmt.Sprintf("SubVec(%d)", invalidSize), func() { postfix.MakeBuilder().SubVec(invalidSize) })
		assertPanics(fmt.Sprintf("MulVec(%d)", invalidSize), func() { postfix.MakeBuilder().MulVec(invalidSize) })
		assertPanics(fmt.Sprintf("DivVec(%d)", invalidSize), func() { postfix.MakeBuilder().DivVec(invalidSize) })
		assertPanics(fmt.Sprintf("DotVec(%d)", invalidSize), func() { postfix.MakeBuilder().DotVec(invalidSize) })
		assertPanics(fmt.Sprintf("NegVec(%d)", invalidSize), func() { postfix.MakeBuilder().NegVec(invalidSize) })
		assertPanics(fmt.Sprintf("InvVec(%d)", invalidSize), func() { postfix.MakeBuilder().InvVec(invalidSize) })
		assertPanics(fmt.Sprintf("NormSqVec(%d)", invalidSize), func() { postfix.MakeBuilder().NormSqVec(invalidSize) })
		assertPanics(fmt.Sprintf("SumVec(%d)", invalidSize), func() { postfix.MakeBuilder().SumVec(invalidSize) })
		assertPanics(fmt.Sprintf("MulAddVec(%d)", invalidSize), func() { postfix.MakeBuilder().MulAddVec(invalidSize) })
		assertPanics(fmt.Sprintf("ScaleVec(%d)", invalidSize), func() { postfix.MakeBuilder().ScaleVec(invalidSize) })
	}

	assertPanics("MulMat row 0", func() { postfix.MakeBuilder().MulMat(0, 3, 1, 2, 3) })
	assertPanics("MulMat row 5", func() { postfix.MakeBuilder().MulMat(5, 1) })
	assertPanics("MulMat col 0", func() { postfix.MakeBuilder().MulMat(1, 0) })
	assertPanics("MulMat col 17", func() { postfix.MakeBuilder().MulMat(1, 17) })
	assertPanics("MulMat element mismatch", func() { postfix.MakeBuilder().MulMat(2, 2, 1, 2, 3) })

	// Verify size - 1 is encoded
	expr := postfix.MakeBuilder().AddVec(3).Build()
	if len(expr.Prog) != 3 || expr.Prog[0] != uint8(postfix.Operation_AddVec) || expr.Prog[1] != 2 || expr.Prog[2] != uint8(postfix.Operation_Ret) {
		t.Errorf("expected Prog [Operation_AddVec, 2, Operation_Ret], got %v", expr.Prog)
	}
	expr1 := postfix.MakeBuilder().AddVec(1).Build()
	if len(expr1.Prog) != 3 || expr1.Prog[1] != 0 || expr1.Prog[2] != uint8(postfix.Operation_Ret) {
		t.Errorf("expected size 1 to encode operand 0, got %v", expr1.Prog)
	}
	expr255 := postfix.MakeBuilder().AddVec(255).Build()
	if len(expr255.Prog) != 3 || expr255.Prog[1] != 254 || expr255.Prog[2] != uint8(postfix.Operation_Ret) {
		t.Errorf("expected size 255 to encode operand 254, got %v", expr255.Prog)
	}
}

func TestOperationString(t *testing.T) {
	if got := postfix.Operation_Push.String(); got != "Push" {
		t.Errorf("got %q, want %q", got, "Push")
	}
	if got := postfix.Operation_Move.String(); got != "Move" {
		t.Errorf("got %q, want %q", got, "Move")
	}
	if got := postfix.Operation_Copy.String(); got != "Copy" {
		t.Errorf("got %q, want %q", got, "Copy")
	}
	if got := postfix.Operation_CopyBase.String(); got != "CopyBase" {
		t.Errorf("got %q, want %q", got, "CopyBase")
	}
	if got := postfix.Operation_Slide.String(); got != "Slide" {
		t.Errorf("got %q, want %q", got, "Slide")
	}
	if got := postfix.Operation_PushVec.String(); got != "PushVec" {
		t.Errorf("got %q, want %q", got, "PushVec")
	}
	if got := postfix.Operation_DotVec.String(); got != "DotVec" {
		t.Errorf("got %q, want %q", got, "DotVec")
	}
	if got := postfix.Operation_Ipow.String(); got != "Ipow" {
		t.Errorf("got %q, want %q", got, "Ipow")
	}
	if got := postfix.Operation_Poly.String(); got != "Poly" {
		t.Errorf("got %q, want %q", got, "Poly")
	}
	if got := postfix.Operation_CosSin.String(); got != "CosSin" {
		t.Errorf("got %q, want %q", got, "CosSin")
	}
	if got := postfix.Operation_PanTilt.String(); got != "PanTilt" {
		t.Errorf("got %q, want %q", got, "PanTilt")
	}
	if got := postfix.Operation_Call.String(); got != "Call" {
		t.Errorf("got %q, want %q", got, "Call")
	}
	if got := postfix.Operation_Piecewise.String(); got != "Piecewise" {
		t.Errorf("got %q, want %q", got, "Piecewise")
	}
	if got := postfix.Operation_Ret.String(); got != "Ret" {
		t.Errorf("got %q, want %q", got, "Ret")
	}
}

func TestCallAndPiecewise(t *testing.T) {
	t.Run("call basic", func(t *testing.T) {
		b := postfix.MakeBuilder()
		sq := b.DefineSubroutine(func(sub *postfix.Builder) {
			sub.Dup().Mul()
		})
		b.Push(5.0).Call(sq)
		expr := b.Build()

		minCap, finalLen, _, err := postfix.Validate(expr, 0)
		if err != nil {
			t.Fatalf("unexpected validation error: %v", err)
		}
		if finalLen != 1 {
			t.Fatalf("want finalLen 1, got %d", finalLen)
		}
		stack := expr.Evaluate(nil)
		if len(stack) != 1 || stack[0] != 25.0 {
			t.Fatalf("got %v, want [25.0]", stack)
		}
		_ = minCap
	})

	t.Run("call nested", func(t *testing.T) {
		b := postfix.MakeBuilder()
		addOne := b.DefineSubroutine(func(sub *postfix.Builder) {
			sub.Push(1.0).Add()
		})
		addTwo := b.DefineSubroutine(func(sub *postfix.Builder) {
			sub.Call(addOne).Call(addOne)
		})
		b.Push(10.0).Call(addTwo)
		expr := b.Build()

		_, finalLen, _, err := postfix.Validate(expr, 0)
		if err != nil {
			t.Fatalf("unexpected validation error: %v", err)
		}
		if finalLen != 1 {
			t.Fatalf("want finalLen 1, got %d", finalLen)
		}
		stack := expr.Evaluate(nil)
		if len(stack) != 1 || stack[0] != 12.0 {
			t.Fatalf("got %v, want [12.0]", stack)
		}
	})

	t.Run("piecewise relative", func(t *testing.T) {
		testVal := func(inX float64) float64 {
			b := postfix.MakeBuilder()
			b0 := b.DefineSubroutine(func(sub *postfix.Builder) {
				sub.Push(1.0).Nip()
			})
			b1 := b.DefineSubroutine(func(sub *postfix.Builder) {
				sub.Dup().Mul()
			})
			b2 := b.DefineSubroutine(func(sub *postfix.Builder) {
				sub.Push(100.0).Nip()
			})
			b.Push(inX).PiecewiseRelative([]float32{0.0, 10.0}, []postfix.SubroutineHandle{b0, b1, b2})
			expr := b.Build()

			_, _, _, err := postfix.Validate(expr, 0)
			if err != nil {
				t.Fatalf("validation failed: %v", err)
			}
			stack := expr.Evaluate(nil)
			return stack[0]
		}

		if got := testVal(-5.0); got != 1.0 {
			t.Errorf("x=-5: got %f, want 1.0", got)
		}
		if got := testVal(0.0); got != 0.0 {
			t.Errorf("x=0: got %f, want 0.0", got)
		}
		if got := testVal(3.0); got != 9.0 {
			t.Errorf("x=3: got %f, want 9.0", got)
		}
		if got := testVal(10.0); got != 100.0 {
			t.Errorf("x=10: got %f, want 100.0", got)
		}
		if got := testVal(15.0); got != 100.0 {
			t.Errorf("x=15: got %f, want 100.0", got)
		}
	})

	t.Run("piecewise absolute", func(t *testing.T) {
		testAbs := func(inX float64) float64 {
			b := postfix.MakeBuilder()
			neg := b.DefineSubroutine(func(sub *postfix.Builder) {
				sub.Neg()
			})
			id := b.DefineSubroutine(func(sub *postfix.Builder) {
				// leaves x as-is
			})
			b.Push(inX).Piecewise([]float32{0.0}, []postfix.SubroutineHandle{neg, id})
			expr := b.Build()

			_, _, _, err := postfix.Validate(expr, 0)
			if err != nil {
				t.Fatalf("validation failed: %v", err)
			}
			stack := expr.Evaluate(nil)
			return stack[0]
		}

		if got := testAbs(-7.5); got != 7.5 {
			t.Errorf("got %f, want 7.5", got)
		}
		if got := testAbs(7.5); got != 7.5 {
			t.Errorf("got %f, want 7.5", got)
		}
	})

	t.Run("validation errors", func(t *testing.T) {
		// Mismatched branch stack sizes
		b := postfix.MakeBuilder()
		b0 := b.DefineSubroutine(func(sub *postfix.Builder) {})
		b1 := b.DefineSubroutine(func(sub *postfix.Builder) {
			sub.Push(1.0)
		})
		b.Push(5.0).Piecewise([]float32{0.0}, []postfix.SubroutineHandle{b0, b1})
		expr := b.Build()
		if _, _, _, err := postfix.Validate(expr, 0); err != postfix.ErrUnexpectedStackSize {
			t.Errorf("got err %v, want ErrUnexpectedStackSize", err)
		}
	})
}

func TestRetSemantics(t *testing.T) {
	// Erased flash simulation: 0xFF terminates cleanly
	flashExpr := &postfix.Expression{Prog: []uint8{255, 255}}
	minCap, finalLen, ops, err := postfix.Validate(flashExpr, 2)
	if err != nil {
		t.Fatalf("unexpected validation error: %v", err)
	}
	if minCap != 2 || finalLen != 2 || len(ops) != 1 || ops[0] != postfix.Operation_Ret {
		t.Errorf("unexpected validation result: cap=%d, len=%d, ops=%v", minCap, finalLen, ops)
	}
	gotStack := flashExpr.Evaluate([]float64{10, 20})
	if !equalSlices(gotStack, []float64{10, 20}) {
		t.Errorf("got %v, want [10 20]", gotStack)
	}

	// Missing Ret fails validation
	noRetExpr := &postfix.Expression{Prog: []uint8{uint8(postfix.Operation_Push), 0, 0, 0, 0}}
	if _, _, _, err := postfix.Validate(noRetExpr, 0); err != postfix.ErrIllegalOperation {
		t.Errorf("got err %v, want ErrIllegalOperation", err)
	}
}
