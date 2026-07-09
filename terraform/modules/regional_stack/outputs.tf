output "api_invoke_url" {
  value = aws_apigatewayv2_stage.default.invoke_url
}

output "cloudfront_domain_name" {
  value = aws_cloudfront_distribution.frontend.domain_name
}
