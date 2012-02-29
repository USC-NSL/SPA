#define NETCALC_UDP_PORT 3141

typedef enum {
	NC_ADDITION = 0,
	NC_SUBTRACTION,
	NC_MUTIPLICATION,
	NC_DIVISION,
	NC_MODULO,

	NC_OPERATOR_END
} nc_operator_t;

std::string nc_operator_names[] = {
	"+",
	"-",
	"*",
	"/",
	"%",

	"[Unknown Operator]"
};

std::string getOpName( nc_operator_t op ) {
	if ( op < NC_OPERATOR_END )
		return nc_operator_names[op];
	else
		return nc_operator_names[NC_OPERATOR_END];
}

nc_operator_t getOpByName( std::string name ) {
	for ( int i = 0; i < NC_OPERATOR_END; i++ )
		if ( nc_operator_names[i] == name )
			return (nc_operator_t) i;
	return NC_OPERATOR_END;
}

typedef long nc_value_t;

typedef struct {
	nc_operator_t op;
	nc_value_t arg1;
	nc_value_t arg2;
} nc_query_t;

typedef enum {
	NC_OK = 0,
	NC_BADINPUT,
	NC_DIV0,

	NC_ERROR_END
} nc_error_t;

std::string nc_error_text[] = {
	"OK.",
	"Bad input.",
	"Division by zero.",

	"Unknown error."
};

std::string getErrText( nc_error_t err ) {
	if ( err < NC_ERROR_END )
		return nc_error_text[err];
	else
		return nc_error_text[NC_ERROR_END];
}

typedef struct {
	nc_error_t err;
	nc_value_t value;
} nc_response_t;
