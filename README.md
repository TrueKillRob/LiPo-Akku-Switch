# LiPo-Akku-Switch

Some devices use LiPo batteries that are connected in parallel and are used to power portable devices.
The calculated capacities achieved in this way can often be reached using one current cell. In addition, the parallel connection of batteries is not ideal, since the cells age at different rates over time and thus drift apart.
The parallel connection results in cross currents which are simply a waste of energy.

In this project, I want to use a small circuit to ensure that the cells no longer have to be connected in parallel. An intelligent management supplies the device always by one cell and it is also ensured for an even discharge.
This will significantly increase the usage time. If it is detected that the batteries are being charged (via the charging socket included in the device), it will also react and ensure that the charge is as even as possible.

