# Example: Hello World in Arcana bytecode assembly
#
# Computes 5 + 10 and prints the result.
# Run: arcana-asm examples/hello.asm -o hello.mgc && arcana-run hello.mgc

.const #0 i64 5
.const #1 i64 10

.func main arity=0 locals=0 max_stack=3
    const 0
    const 1
    add
    dup
    intrinsic 0 1
    halt
.end
