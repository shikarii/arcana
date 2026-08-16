; Test assembly: compute 5 + 10
.const #0 string "main"
.const #1 i64 5
.const #2 i64 10

.func main arity=0 locals=0 max_stack=2
  const #1
  const #2
  add
  halt
.end
