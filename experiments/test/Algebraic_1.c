//Algebraic identity examples
int compute (int a, int b)
{
  int result = (a/a); // result = 1

  result *= (b/b); // result = 1 * 1
  result += (b-b); // result = 1 + 0
  result /= result; // result = 1 / 1
  result -= result; // result = 1 - 1
  result += 23; //constant folding
  return result;
}
