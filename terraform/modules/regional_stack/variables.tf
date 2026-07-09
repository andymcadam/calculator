variable "name_prefix" {
  type = string
}

variable "table_name" {
  type = string
}

variable "region" {
  type = string
}

variable "lambda_source_file" {
  type = string
}

variable "frontend_dir" {
  type = string
}

variable "cloudwatch_log_retention_days" {
  type = number
}

variable "history_default_limit" {
  type = number
}

variable "history_max_limit" {
  type = number
}
