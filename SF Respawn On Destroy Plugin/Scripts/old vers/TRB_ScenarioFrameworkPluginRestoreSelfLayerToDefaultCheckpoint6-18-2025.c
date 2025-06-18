// Enum to define the trigger modes available in the editor dropdown.
enum ETRB_RestoreTriggerMode
{
	ON_DESTROY,
	TIMER
}

[BaseContainerProps(), SCR_ContainerActionTitle()]
class TRB_ScenarioFrameworkPluginRestoreSelfLayer : SCR_ScenarioFrameworkPlugin
{
	// --- NEW: Attribute to select the trigger mode ---
	[Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, desc: "How the layer restoration is triggered.", category: "Trigger", enums: ParamEnumArray.FromEnum(ETRB_RestoreTriggerMode))]
	protected ETRB_RestoreTriggerMode m_eTriggerMode;
	
	// --- Attributes for Delay Logic ---
	[Attribute(defvalue: "5", desc: "How long to wait in seconds before restoring the layer.", category: "Delay")]
	int m_iDelayInSeconds;

	[Attribute(defvalue: "10", desc: "If this is set to a number larger than Delay In Seconds, it will randomize the delay between these two values.", category: "Delay")]
	int m_iDelayInSecondsMax;
	
	[Attribute(UIWidgets.CheckBox, desc: "If true, it will attempt to restore the layer in a loop. Use with caution.", category: "Delay")]
	bool m_bLooped;

	// --- Attributes for Restoration Logic ---
	[Attribute(defvalue: "true", desc: "If checked, it will also restore child layers to default state as well.", category: "Restoration")]
	bool m_bIncludeChildren;
	
	[Attribute(defvalue: "true", desc: "If checked, it will reinit the layer after the restoration.", category: "Restoration")]
	bool m_bReinitAfterRestoration;
	
	[Attribute(defvalue: "true", desc: "If checked, it will also clear randomization and re-randomize it again.", category: "Restoration")]
	bool m_bAffectRandomization;

	protected IEntity m_Asset; // The entity this plugin is monitoring (e.g., the vehicle)
	SCR_ScenarioFrameworkLayerBase base_layer_object;
	//------------------------------------------------------------------------------------------------
	//! Set up the damage listeners on the spawned entity OR starts the timer.
	override void Init(SCR_ScenarioFrameworkLayerBase object)
	{
		if (!object)
			return;
		super.Init(object); // This sets 'm_Object.GetOwner()', which is the layer itself.
		base_layer_object = object;
		
		// --- MODIFIED: Use a switch to handle the selected trigger mode ---
		switch (m_eTriggerMode)
		{
			// --- Case 1: Original OnDestroy logic ---
			case ETRB_RestoreTriggerMode.ON_DESTROY:
			{
				IEntity entity = object.GetSpawnedEntity();
				if (!entity)
				{
					Print(string.Format("ScenarioFramework: Restore Layer plugin on '%1' is set to ON_DESTROY but the layer has no spawned entity to monitor!", object.GetName()), LogLevel.ERROR);
					return;
				}

				m_Asset = entity;
				SCR_DamageManagerComponent objectDmgManager = SCR_DamageManagerComponent.GetDamageManager(m_Asset);
				if (objectDmgManager)
					objectDmgManager.GetOnDamageStateChanged().Insert(OnObjectDamage);
				else
					PrintFormat("ScenarioFramework: Registering OnDestroy for entity %1 failed! It lacks a damage manager.", entity, LogLevel.ERROR);

				// Special handling for vehicle engine drowning
				if (Vehicle.Cast(m_Asset))
				{
					VehicleControllerComponent vehicleController = VehicleControllerComponent.Cast(m_Asset.FindComponent(VehicleControllerComponent));
					if (vehicleController)
						vehicleController.GetOnEngineStop().Insert(CheckEngineDrowned);

					SCR_ScenarioFrameworkSystem.GetCallQueuePausable().CallLater(CheckEngineDrowned, 5000, true);
				}
				break;
			}
			
			// --- Case 2: New Timer logic ---
			case ETRB_RestoreTriggerMode.TIMER:
			{
				// Directly schedule the restoration without waiting for an event.
				ScheduleRestore();
				break;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Called when the asset's damage state changes. (Only used in ON_DESTROY mode)
	protected void OnObjectDamage(EDamageState state)
	{
		if (m_bDebug)
			Print(string.Format("[SCR_PluginRestoreLayer] OnObjectDamage triggered for %1", m_Asset.GetName()), LogLevel.DEBUG);
		
		if (state == EDamageState.DESTROYED && m_Asset)
		{
			CleanupListeners();
			ScheduleRestore();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Called periodically to check if a vehicle's engine has drowned. (Only used in ON_DESTROY mode)
	protected void CheckEngineDrowned()
	{
		if (!m_Asset)
			return;

		VehicleControllerComponent vehicleController = VehicleControllerComponent.Cast(m_Asset.FindComponent(VehicleControllerComponent));
		if (vehicleController && vehicleController.GetEngineDrowned())
		{
			if (m_bDebug)
				Print(string.Format("[SCR_PluginRestoreLayer] Engine drowned for %1", m_Asset.GetName()), LogLevel.DEBUG);
			
			CleanupListeners();
			ScheduleRestore();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Helper method to remove all event listeners to prevent multiple triggers. (Only used in ON_DESTROY mode)
	protected void CleanupListeners()
	{
		if (!m_Asset)
			return;
			
		SCR_DamageManagerComponent objectDmgManager = SCR_DamageManagerComponent.GetDamageManager(m_Asset);
		if (objectDmgManager)
			objectDmgManager.GetOnDamageStateChanged().Remove(OnObjectDamage);
		
		SCR_ScenarioFrameworkSystem.GetCallQueuePausable().Remove(CheckEngineDrowned);

		VehicleControllerComponent vehicleController = VehicleControllerComponent.Cast(m_Asset.FindComponent(VehicleControllerComponent));
		if (vehicleController)
			vehicleController.GetOnEngineStop().Remove(CheckEngineDrowned);
	}

	//------------------------------------------------------------------------------------------------
	//! Calculates the delay and schedules the RestoreLayer() method to be called.
	protected void ScheduleRestore()
	{
		int delay = m_iDelayInSeconds;
		if (m_iDelayInSecondsMax > m_iDelayInSeconds)
			delay = Math.RandomIntInclusive(m_iDelayInSeconds, m_iDelayInSecondsMax);

		// --- MODIFIED: Log message now accounts for both trigger modes ---
		string logMessage;
		if (m_eTriggerMode == ETRB_RestoreTriggerMode.ON_DESTROY && m_Asset)
		{
			logMessage = string.Format("Entity '%1' destroyed. Scheduling layer restoration for '%2' in %3 seconds.", m_Asset.GetName(), m_Object.GetOwner().GetName(), delay);
		}
		else
		{
			logMessage = string.Format("Timer trigger activated. Scheduling layer restoration for '%1' in %2 seconds.", m_Object.GetOwner().GetName(), delay);
		}
		Print(logMessage, LogLevel.NORMAL);
		
		SCR_ScenarioFrameworkSystem.GetCallQueuePausable().CallLater(RestoreLayer, delay * 1000, m_bLooped);
	}

	//------------------------------------------------------------------------------------------------
	//! The method that performs the actual layer restoration.
	protected void RestoreLayer()
	{
		if (!base_layer_object) // m_Owner is the layer this plugin is on, set by super.Init()
		{
			Print("Cannot restore layer: The plugin's owner is not valid.", LogLevel.ERROR);
			return;
		}

		Print(string.Format("Executing restoration for layer '%1'.", base_layer_object.GetName()), LogLevel.NORMAL);
		base_layer_object.RestoreToDefault(m_bIncludeChildren, m_bReinitAfterRestoration, m_bAffectRandomization);
	}
}