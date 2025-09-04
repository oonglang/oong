// benchmark.js
function fib(n) {
  return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

const start = Date.now();
const result = fib(35);
const end = Date.now();

console.log("Result:", result);
console.log("Time taken:", end - start, "ms");
