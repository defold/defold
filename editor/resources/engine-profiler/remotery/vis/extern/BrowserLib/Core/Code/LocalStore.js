
namespace("LocalStore");


LocalStore.Set = function(class_name, class_id, variable_id, data)
{
	try
	{
		if (typeof(Storage) != "undefined")
		{
			var name = class_name + "_" + class_id + "_" + variable_id;
			localStorage[name] = JSON.stringify(data);
		}
	}
	catch (e)
	{
		console.log("Local Storage Set Error: " + e.message);
	}
}


LocalStore.Get = function(class_name, class_id, variable_id, default_data)
{
	try
	{
		if (typeof(Storage) != "undefined")
		{
			var name = class_name + "_" + class_id + "_" + variable_id;
			var data = localStorage[name]
			if (data)
				return JSON.parse(data);
		}
	}
	catch (e)
	{
		console.log("Local Storage Get Error: " + e.message);
	}

	return default_data;
}