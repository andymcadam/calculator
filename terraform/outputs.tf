output "history_table_name" {
  value       = module.global_data.table_name
  description = "DynamoDB global table used for calculation history"
}

output "primary_api_url" {
  value       = module.regional_primary.api_invoke_url
  description = "Primary region API endpoint"
}

output "secondary_api_url" {
  value       = module.regional_secondary.api_invoke_url
  description = "Secondary region API endpoint"
}

output "primary_cloudfront_domain" {
  value       = module.regional_primary.cloudfront_domain_name
  description = "Primary CloudFront distribution domain"
}

output "secondary_cloudfront_domain" {
  value       = module.regional_secondary.cloudfront_domain_name
  description = "Secondary CloudFront distribution domain"
}
