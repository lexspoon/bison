# modes - common ways to use lexical modes

This examples shows how to use lexical modes, using two common
examples.

First, it shows how to parse string literals like `"foo\nbar"` by
switching to a different mode when it sees the first `"` character,
and switching back when it sees the last `"` character. A mode works
well for this kind of syntax because there is a completely separate
set of tokens inside the quotes than outside.

Second, it shows how to parse and discard comments that are enclosed
by `/*` and `*/`. Again, the lexing rules inside a comment are
different from whatever exists in the rest of the language, so it
works well to use a mode.

This second part of the example also shows how to use a mode stack.
The comments in this example are allowed to nest, which is a trick
some languages use to let you comment out any code by putting `/*` and
`*/` around it, even if the code you are commenting out already
contains comments in it. Accomplishing this is done with a mode
stack. Each `/*` causes a push onto the mode stack, and each `*/`
causes a pop. The full, outermost comment has ended when the last `*/`
is seen.
