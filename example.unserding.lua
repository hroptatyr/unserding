-- unserding example config

-- database resources
db_local = {
	type = MYSQL,
	host = "::1",
	port = 3306,
	user = "GAT_user",
	pass = "EFGau5A4A5BLGAme",
	schema = "freundt",
}

db_cobain = {
	type = MYSQL,
	host = "cobain",
	port = 3306,
	user = "GAT_user",
	pass = "EFGau5A4A5BLGAme",
	schema = "freundt",
}

-- define the instr fetcher guts
instr_fetcher = {
	file = "dso-xdr-instr.la",
	source = db_local,
}

-- define the ticks fetcher guts
ticks_fetcher = {
	file = "dso-tseries.la",
	source = db_local,
}

-- now the actual control flow, load_module() is a C primitive
-- load instr module
load_module(instr_fetcher);

-- load ticks module
load_module(ticks_fetcher);


