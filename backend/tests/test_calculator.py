import unittest
from decimal import Decimal

from lambda_function import CalculatorError, evaluate_expression


class EvaluateExpressionTests(unittest.TestCase):
    def test_evaluate_expression_basic_math(self):
        self.assertEqual(evaluate_expression("2+3*4"), Decimal("14"))

    def test_evaluate_expression_parentheses(self):
        self.assertEqual(evaluate_expression("(2+3)*4"), Decimal("20"))

    def test_evaluate_expression_division_by_zero(self):
        with self.assertRaises(CalculatorError):
            evaluate_expression("1/0")

    def test_evaluate_expression_rejects_invalid(self):
        with self.assertRaises(CalculatorError):
            evaluate_expression("__import__('os').system('id')")


if __name__ == "__main__":
    unittest.main()
