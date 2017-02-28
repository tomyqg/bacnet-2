# Export the variables defined here to all subprocesses
.EXPORT_ALL_VARIABLES:

all: debug_test bip_test readprop readpropm readrange writeprop writepropm trendlog_test web_client_test web_server_test webui my_test
.PHONY : all clean debug_test bip_test readprop readpropm readrange writeprop writepropm trendlog_test web_client_test web_server_test webui my_test

debug_test:
	$(MAKE) -C debug_test all

bip_test:
	$(MAKE) -C bip_test all

readprop:
	$(MAKE) -C readprop all

readpropm:
	$(MAKE) -C readpropm all

readrange:
	$(MAKE) -C readrange all

writeprop:
	$(MAKE) -C writeprop all

writepropm:
	$(MAKE) -C writepropm all

trendlog_test:
	$(MAKE) -C trendlog_test all

web_client_test:
	$(MAKE) -C web_client_test all

web_server_test:
	$(MAKE) -C web_server_test all

webui:
	$(MAKE) -C webui all

my_test:
	$(MAKE) -C my_test all

clean:
	-$(MAKE) -C debug_test clean
	-$(MAKE) -C bip_test clean
	-$(MAKE) -C readprop clean
	-$(MAKE) -C readpropm clean
	-$(MAKE) -C readrange clean
	-$(MAKE) -C writeprop clean
	-$(MAKE) -C writepropm clean
	-$(MAKE) -C trendlog_test clean
	-$(MAKE) -C web_client_test clean
	-$(MAKE) -C web_server_test clean
	-$(MAKE) -C webui clean
	-$(MAKE) -C my_test clean
