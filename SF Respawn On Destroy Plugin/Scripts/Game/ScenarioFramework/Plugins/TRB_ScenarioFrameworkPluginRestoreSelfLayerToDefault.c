[BaseContainerProps(), SCR_ContainerActionTitle()]
class TRB_ScenarioFrameworkPluginRestoreSelfLayerOnDestroy : SCR_ScenarioFrameworkPlugin
{
	// --- Attributes for Delay Logic ---
	[Attribute(defvalue: "5", desc: "How long to wait in seconds before restoring the layer.", category: "Delay")]
	int m_iDelayInSeconds;

	[Attribute(defvalue: "10", desc: "If this is set to a number larger than Delay In Seconds, it will randomize the delay between these two values.", category: "Delay")]
	int m_iDelayInSecondsMax;
	
	// Note: Looping on a single OnDestroy event is not logical, as an entity is only destroyed once.
	// This is included for completeness but should likely remain false.
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
	//! Set up the damage listeners on the spawned entity.
	override void Init(SCR_ScenarioFrameworkLayerBase object)
	{
		if (!object)
			return;
		super.Init(object); // This sets 'm_Object.GetOwner()', which is the layer itself.
		base_layer_object = object;
		IEntity entity = object.GetSpawnedEntity();
		if (!entity)
			return;

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
	}

	//------------------------------------------------------------------------------------------------
	//! Called when the asset's damage state changes.
	protected void OnObjectDamage(EDamageState state)
	{
		if (m_bDebug)
			Print(string.Format("[SCR_PluginRestoreLayerOnDestroy] OnObjectDamage triggered for %1", m_Asset.GetName()), LogLevel.DEBUG);
		
		if (state == EDamageState.DESTROYED && m_Asset)
		{
			CleanupListeners();
			ScheduleRestore();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Called periodically and on engine stop to check if a vehicle's engine has drowned.
	protected void CheckEngineDrowned()
	{
		if (!m_Asset)
			return;

		VehicleControllerComponent vehicleController = VehicleControllerComponent.Cast(m_Asset.FindComponent(VehicleControllerComponent));
		if (vehicleController && vehicleController.GetEngineDrowned())
		{
			if (m_bDebug)
				Print(string.Format("[SCR_PluginRestoreLayerOnDestroy] Engine drowned for %1", m_Asset.GetName()), LogLevel.DEBUG);
			
			CleanupListeners();
			ScheduleRestore();
		}
	}
    
    //------------------------------------------------------------------------------------------------
    //! Helper method to remove all event listeners to prevent multiple triggers.
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

		Print(string.Format("Entity '%1' destroyed. Scheduling layer restoration for '%2' in %3 seconds.", m_Asset.GetName(), m_Object.GetOwner().GetName(), delay), LogLevel.DEBUG);

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

		Print(string.Format("Executing restoration for layer '%1'.", base_layer_object.GetName()), LogLevel.DEBUG);
		base_layer_object.RestoreToDefault(m_bIncludeChildren, m_bReinitAfterRestoration, m_bAffectRandomization);
	}
}