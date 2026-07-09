variable "table_name" {
  description = "DynamoDB table name"
  type        = string
}

variable "secondary_region" {
  description = "Secondary region for global table replica"
  type        = string
}
