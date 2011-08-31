-- unserding example config
-- Note, the modules dso-xdr-instr.la and dso-tseries.la are not
-- part of unserding.  Also, anything but the `file' slot is up to
-- the module developer and the module documentation should be
-- considered for slot names and their meaning and purpose.
-- This is just to demonstrate that several modules can be loaded
-- simultaneously and certain aspects of configuration can be
-- shared between them.

-- database resources
db_local = {
	type = "mysql",
	host = "::1",
	port = 3306,
	user = "testuser",
	pass = "testpass",
}

db_cobain = {
	type = "mysql",
	host = "dbbox",
	port = 3306,
	user = "testuser",
	pass = "testpass",
	schema = "test",
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

-- example.unserding.lua ends here
