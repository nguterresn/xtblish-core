export function hello_world(): void {
  console.log(
    // v Debug only! v
    `Hello world baby! __data_end=${__data_end} __stack_pointer=${__stack_pointer} __heap_base=${__heap_base} pages=${memory.size()}`
  );
}
