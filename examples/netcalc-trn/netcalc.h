#define NETCALC_UDP_PORT 3141

typedef enum {
  NC_CAPABILITY = 0,
  NC_ADDITION,
  NC_SUBTRACTION,
  NC_MUTIPLICATION,
  NC_DIVISION,
  NC_MODULO,

  NC_OPERATOR_END
} nc_operator_t;

// Convert operator to it name.
std::string getOpName(nc_operator_t op) {
  switch (op) {
  case NC_CAPABILITY:
    return "Capabilities";
  case NC_ADDITION:
    return "+";
  case NC_SUBTRACTION:
    return "-";
  case NC_MUTIPLICATION:
    return "*";
  case NC_DIVISION:
    return "/";
  case NC_MODULO:
    return "%";
  default:
    return "[Unknown Operator]";
  }
}

// Convert an operator name to the operator enum.
nc_operator_t getOpByName(std::string name) {
  if ("+" == name)
    return NC_ADDITION;
  if ("-" == name)
    return NC_SUBTRACTION;
  if ("*" == name)
    return NC_MUTIPLICATION;
  if ("/" == name)
    return NC_DIVISION;
  if ("%" == name)
    return NC_MODULO;
  return NC_OPERATOR_END;
}

typedef enum {
  NC_OK = 0,
  NC_BADINPUT,
  NC_DIV0,

  NC_ERROR_END
} nc_error_t;

// Convert an error enum to a human readable description.
std::string getErrText(nc_error_t err) {
  switch (err) {
  case NC_OK:
    return "OK.";
  case NC_BADINPUT:
    return "Bad input.";
  case NC_DIV0:
    return "Division by zero.";
  default:
    return "Unknown error.";
  }
}

typedef long nc_value_t;

typedef struct {
  nc_operator_t op;
  nc_value_t arg1;
  nc_value_t arg2;
} nc_query_t;

typedef struct {
  nc_error_t err;
  nc_value_t value;
} nc_response_t;
