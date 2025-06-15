# Arma Reforger ScenarioFramework Resawn Helper Plugin

This is a mod for Arma Reforger. It does not do anyting to just install it, this is a utility for modders specifially using the ScenarioFramework. It is essentially a small bundle of built-in functions all rolled up into a more convenient wrapper. 


## What does it do?

Adding this Plugin to a Slot that spawns an Entity will cause that Entity to respawn after it is destroyed. This differs from the existing methods by automatically targetting the entity the plugin is attached too, rather than having to specify a layer explicitly with SCR_ScenarioFrameworkActionRestoreLayerToDefault. This is ideal for copying and pasting many spawners with minimal fidgeting afterward. 


## How do I use it?

Prerequisits:
- Have a World open in the Enfusion Workbench World Editor
- In the top menu under Plugins, make sure to run Game Mode Setup with the ScenarioFramework.conf template, and/or setup your World for ScenarioFramework manually

To use this helper plugin:
1. Create your standard Area > Layer > Slot setup
2. Setup your Slot to spawn your Entity (tested with Characters and Vehicles, also works with randomization)
3. Under Plugins add TRB_ScenarioFrameworkPluginRestoreSelfLayerOnDestroy
4. Adjust the delay and other settings to your liking
