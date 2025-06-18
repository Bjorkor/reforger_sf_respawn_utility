// ---- HELPER ACTION CLASS --------------------------------------------------------------------
// This is a minimal helper class defined in the same file. 
// Its *only* purpose is to give our plugin access to the ValidateInputEntity function.
// It is not intended to be used directly in the editor.
[BaseContainerProps()]
class TRB_RestoreLayerHelperAction : SCR_ScenarioFrameworkActionBase
{
	//------------------------------------------------------------------------------------------------
	//! A public method that safely wraps the ValidateInputEntity call.
	//! \param context The entity to use as a fallback if the getter is null.
	//! \param getter The getter we want to resolve.
	//! \return The IEntity of the found layer, or null if not found.
	IEntity ResolveLayerGetter(IEntity context, SCR_ScenarioFrameworkGetLayerBase getter)
	{
		IEntity resolvedEntity;
		if (ValidateInputEntity(context, getter, resolvedEntity))
		{
			return resolvedEntity;
		}
		return null;
	}
}


// ---- MAIN PLUGIN CLASS ----------------------------------------------------------------------

// Enum to define the trigger modes available in the editor dropdown.
enum ETRB_RestoreTriggerMode
{
	ON_DESTROY,
	TIMER
}

// Enum to define the target layer mode.
enum ETRB_TargetMode
{
	SELF,
	CUSTOM_LAYER
}

[BaseContainerProps(), SCR_ContainerActionTitle()]
class TRB_ScenarioFrameworkPluginRestoreLayer : SCR_ScenarioFrameworkPlugin
{
	[Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, desc: "How the layer restoration is triggered.", category: "Trigger", enums: ParamEnumArray.FromEnum(ETRB_RestoreTriggerMode))]
	protected ETRB_RestoreTriggerMode m_eTriggerMode;
	
	[Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, desc: "Which layer to restore.\nSELF: Restores the layer this plugin is attached to.\nCUSTOM_LAYER: Restores a different layer specified below.", category: "Target", enums: ParamEnumArray.FromEnum(ETRB_TargetMode))]
	protected ETRB_TargetMode m_eTargetMode;

	[Attribute(desc: "Layer to be restored. \nUsed only when Target Mode is set to CUSTOM_LAYER.", category: "Target")]
	protected ref SCR_ScenarioFrameworkGetLayerBase m_LayerGetter;

	[Attribute(defvalue: "5", desc: "How long to wait in seconds before restoring the layer.", category: "Delay")]
	protected int m_iDelayInSeconds;

	[Attribute(defvalue: "10", desc: "If this is set to a number larger than Delay In Seconds, it will randomize the delay between these two values.", category: "Delay")]
	protected int m_iDelayInSecondsMax;
	
	[Attribute(UIWidgets.CheckBox, desc: "If true, it will attempt to restore the layer in a loop. Use with caution.", category: "Delay")]
	protected bool m_bLooped;

	[Attribute(defvalue: "true", desc: "If checked, it will also restore child layers to default state as well.", category: "Restoration")]
	protected bool m_bIncludeChildren;
	
	[Attribute(defvalue: "true", desc: "If checked, it will reinit the layer after the restoration.", category: "Restoration")]
	protected bool m_bReinitAfterRestoration;
	
	[Attribute(defvalue: "true", desc: "If checked, it will also clear randomization and re-randomize it again.", category: "Restoration")]
	protected bool m_bAffectRandomization;

	protected IEntity m_Asset;
	protected SCR_ScenarioFrameworkLayerBase m_LocalLayerObject;
	
	// A persistent instance of our helper class.
	protected ref TRB_RestoreLayerHelperAction m_HelperAction;
	
	//------------------------------------------------------------------------------------------------
	override void Init(SCR_ScenarioFrameworkLayerBase object)
	{
		if (!object)
			return;
			
		super.Init(object);
		m_LocalLayerObject = object;
		
		// Create an instance of our helper once.
		m_HelperAction = new TRB_RestoreLayerHelperAction();
		
		switch (m_eTriggerMode)
		{
			case ETRB_RestoreTriggerMode.ON_DESTROY:
			{
				IEntity entity = m_LocalLayerObject.GetSpawnedEntity();
				if (!entity)
				{
					Print(string.Format("ScenarioFramework: Restore Layer plugin on '%1' is set to ON_DESTROY but the layer has no spawned entity to monitor!", m_LocalLayerObject.GetName()), LogLevel.ERROR);
					return;
				}

				m_Asset = entity;
				SCR_DamageManagerComponent objectDmgManager = SCR_DamageManagerComponent.GetDamageManager(m_Asset);
				if (objectDmgManager)
					objectDmgManager.GetOnDamageStateChanged().Insert(OnObjectDamage);
				else
					PrintFormat("ScenarioFramework: Registering OnDestroy for entity %1 failed! It lacks a damage manager.", entity, LogLevel.ERROR);

				if (Vehicle.Cast(m_Asset))
				{
					VehicleControllerComponent vehicleController = VehicleControllerComponent.Cast(m_Asset.FindComponent(VehicleControllerComponent));
					if (vehicleController)
						vehicleController.GetOnEngineStop().Insert(CheckEngineDrowned);

					SCR_ScenarioFrameworkSystem.GetCallQueuePausable().CallLater(CheckEngineDrowned, 5000, true);
				}
				break;
			}
			
			case ETRB_RestoreTriggerMode.TIMER:
			{
				ScheduleRestore();
				break;
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void ScheduleRestore()
	{
		int delay = m_iDelayInSeconds;
		if (m_iDelayInSecondsMax > m_iDelayInSeconds)
			delay = Math.RandomIntInclusive(m_iDelayInSeconds, m_iDelayInSecondsMax);

		Print(string.Format("Scheduling layer restoration in %1 seconds.", delay), LogLevel.NORMAL);
		SCR_ScenarioFrameworkSystem.GetCallQueuePausable().CallLater(RestoreLayer, delay * 1000, m_bLooped);
	}

	//------------------------------------------------------------------------------------------------
	protected void RestoreLayer()
	{
		SCR_ScenarioFrameworkLayerBase layerToRestore;
		
		switch (m_eTargetMode)
		{
			case ETRB_TargetMode.SELF:
			{
				layerToRestore = m_LocalLayerObject;
				break;
			}
			
			case ETRB_TargetMode.CUSTOM_LAYER:
			{
				if (!m_LayerGetter || !m_HelperAction)
				{
					Print(string.Format("ScenarioFramework: Restore Layer plugin on '%1' is not configured correctly for CUSTOM_LAYER mode.", m_LocalLayerObject.GetName()), LogLevel.ERROR);
					return;
				}
				
				// Use our helper to resolve the getter and get the target entity.
				IEntity targetEntity = m_HelperAction.ResolveLayerGetter(m_LocalLayerObject.GetOwner(), m_LayerGetter);
				
				if (targetEntity)
				{
					layerToRestore = SCR_ScenarioFrameworkLayerBase.Cast(targetEntity.FindComponent(SCR_ScenarioFrameworkLayerBase));
				}
				break;
			}
		}
		
		// Perform the restoration on the layer we found.
		if (layerToRestore)
		{
			Print(string.Format("Executing restoration for layer '%1'.", layerToRestore.GetName()), LogLevel.NORMAL);
			layerToRestore.RestoreToDefault(m_bIncludeChildren, m_bReinitAfterRestoration, m_bAffectRandomization);
		}
		else
		{
			Print(string.Format("ScenarioFramework: Restore Layer plugin on '%1' failed to find a valid layer to restore.", m_LocalLayerObject.GetName()), LogLevel.ERROR);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnObjectDamage(EDamageState state)
	{
		if (state == EDamageState.DESTROYED && m_Asset)
		{
			CleanupListeners();
			ScheduleRestore();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckEngineDrowned()
	{
		if (!m_Asset) return;
		VehicleControllerComponent vehicleController = VehicleControllerComponent.Cast(m_Asset.FindComponent(VehicleControllerComponent));
		if (vehicleController && vehicleController.GetEngineDrowned())
		{
			CleanupListeners();
			ScheduleRestore();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void CleanupListeners()
	{
		if (!m_Asset) return;
		SCR_DamageManagerComponent objectDmgManager = SCR_DamageManagerComponent.GetDamageManager(m_Asset);
		if (objectDmgManager)
			objectDmgManager.GetOnDamageStateChanged().Remove(OnObjectDamage);
		SCR_ScenarioFrameworkSystem.GetCallQueuePausable().Remove(CheckEngineDrowned);
		VehicleControllerComponent vehicleController = VehicleControllerComponent.Cast(m_Asset.FindComponent(VehicleControllerComponent));
		if (vehicleController)
			vehicleController.GetOnEngineStop().Remove(CheckEngineDrowned);
	}
}