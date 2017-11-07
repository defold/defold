# Continuous Integration

http://ci.defold.com/waterfall


# Login to Windows Builders

* Enable WiFi

	* Login to K-Guest (the other networks doesn't allow remote desktops!)

* Goto [sso.king.com](https://sso.king.com)

	* Login to the **"Amazon Web Services - Defold"**

* Select **Services -> EC2** from the menu

* Under **"Resources"**, click **Running instances**

* [Optional] You can filter the list, by e.g. "ci-slave-windows"

	* You should see e.g. **"ci-slave-windows-64-2"** and **"ci-slave-windows-64-3"**

* Select one of the windows builds

	* Copy the **"IPv4 Public IP"**

* Open your remote desktop client

	* E.g. Parallels Client

* Connect to the IP using the remote desktop client

	* User: **"administrator"**
	* Pass: Ask mikael.lothman@king.com for the password

* Done!

