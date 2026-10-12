from dsg_api import DSG
dsg = DSG("COM4", message_handler=print)
dsg.identify()
dsg.set_frequency(2.4, "GHz")
dsg.set_power_level(0)
dsg.set_rf_output(True)
dsg.read_telemetry()
dsg.set_rf_output(False)
dsg.close()