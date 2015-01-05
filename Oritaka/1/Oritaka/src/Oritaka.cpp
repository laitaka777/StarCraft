#include "Oritaka.h"

using namespace BWAPI;
using namespace BWTA;
//using namespace BWSAL;
using namespace std;

struct cmp_buildable_work {
	bool (*ptbl)[ MAX_MAP_WIDTH ][ MAX_MAP_HEIGHT ];
	int width;
	int height;
};

struct building {
	int type; //UniteTypes
	Position place;
};

struct task_unit {
	enum task_order order;
	Position position_target;
	int id_target;
};


struct scout_history {
	bool isEnemyBase;
	int nFrame;
	Position enemy_pos;
};

struct base {
	Position pos;
	int num_patch;
	int num_scv; 
};

struct marine_data {
	int friend_power;
	int enemy_power;
	bool escape;
};

enum task_order {
	task_na,
	task_idle,
	task_scout,
	task_attack,
	task_escape,
	task_mine_mineral,
	task_collect_gas,
	task_set_rallypoint,
	task_player_guard,
	task_balance_scv,
	task_train_scv,
	task_train_marine,
	task_train_firebat,
	task_train_ghost,
	task_train_medic,
	task_train_vulture,
	task_train_tank,
	task_train_goliath,
	task_train_wraith,
	task_train_dropship,
	task_train_battlecruiser,
	task_train_vessel,
	task_train_valkyrie,
	task_build_command_center,
	task_build_supply_depot,
	task_build_refinery,
	task_build_barracks,
	task_build_engineering_bay,
	task_build_missile_turret,
	task_build_academy,
	task_build_bunker,
	task_build_factory,
	task_build_machine_shop,
	task_build_armory,
	task_build_starport,
	task_build_science_facility,
	task_end
};


bool first_base_destroy;
bool table_buildable[ MAX_MAP_WIDTH ][ MAX_MAP_HEIGHT ];
int map_width;
int map_height;
int nscout_history;
struct task_unit table_task[ MAX_ID ];
struct scout_history table_scout_history[ MAX_BASE ];
//struct building  table_building[ MAX_BUILDING ];
struct base table_base[ MAX_BASE ];
struct marine_data marine_data[MAX_ID];
Position places_supply_depot[ MAX_BUILDING ];
Position places_command_center[ MAX_BUILDING ];
Position places_barracks[ MAX_BUILDING ]; 
Position first_base_position;

void update_buildable( void );
bool cmp_buildable( int x, int y, struct cmp_buildable_work *p );
void foo_bresenham( int x1, int y1, int x2, int y2, void (*foo)( int x, int y ) );
void set_buildable( int x, int y );
BWAPI::TilePosition foo_spiral_search( const BWAPI::TilePosition &center, int max_length, struct cmp_buildable_work *p,
									  bool (*foo)( int x, int y, struct cmp_buildable_work *p ) );


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Oritaka::onStart()
{
	Broodwar->sendText("My name is Oritaka!!"); // Sends text to other players - as if it were entered in chat.
	Broodwar->setLocalSpeed(0);

	if(BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran)
		BWAPI::Broodwar->sendText("Oritaka is a Terran only bot.");

	// Enable some cheat flags
	Broodwar->enableFlag(Flag::UserInput);

	/* -------------------starcraft cheat code-------------------
	Power overwhelming - God Mode
	Black Sheep Wall - Shows entire map
	Show me the money - Gives you 10,000 gas and 10,000 crystal
	Operation CWAL - Speeds construction of buildings and units  
	-----------------------------------------------------------*/

	//Broodwar->sendText("power overwhelming");
	//Broodwar->sendText("black sheep wall");
	//Broodwar->sendText("show me the money");
	//Broodwar->sendText("operation cwal");
	

	// Uncomment to enable complete map information
	//Broodwar->enableFlag(Flag::CompleteMapInformation);

	//read map information into BWTA so terrain analysis can be done in another thread
	BWTA::readMap(); 
	BWTA::analyze();

	map_width = Broodwar->mapWidth();
	map_height = Broodwar->mapHeight();

	for( int id = 0	; id < MAX_ID; id++ )
	{
		table_task[id].order = task_na; 
		table_task[id].position_target = Positions::Invalid;
		table_task[id].id_target = 0;
	}

	first_base_position = Positions::Invalid;
	first_base_destroy = false;

	int cnt = 0;
	for( std::set<BWTA::BaseLocation*>::const_iterator b = BWTA::getBaseLocations().begin(); b != BWTA::getBaseLocations().end(); b++, cnt++ )
	{
		table_scout_history[cnt].enemy_pos   = (*b)->getPosition();
		table_scout_history[cnt].isEnemyBase = false;
		table_scout_history[cnt].nFrame      = 0;
	}

	nscout_history = cnt;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Oritaka::onFrame()
{	
	
	//draw status
	
	for(std::set<Unit*>::const_iterator i = Broodwar->self()->getUnits().begin(); i != Broodwar->self()->getUnits().end(); i++)
	{
		Broodwar->drawTextMap((*i)->getPosition().x(),(*i)->getPosition().y(), "\x07%s(%d)[%s]",
			(*i)->getType().getName().c_str(),
			(*i)->getID(),
			(*i)->getOrder().getName().c_str() );
	}

	Broodwar->drawTextScreen(300, 0, "FrameCount %d", Broodwar->getFrameCount());
	Broodwar->drawTextScreen(300, 20, "Second     %d", Broodwar->getFrameCount()/25); 
	Broodwar->drawTextScreen(300, 40, "MapName    %s", Broodwar->mapFileName().c_str());

	std::set<Unit*> myUnits = Broodwar->self()->getUnits();
	Broodwar->drawTextScreen(5,0,"self %d units:", myUnits.size());
	std::map<UnitType, int> unitTypeCounts;

	for(std::set<Unit*>::iterator i = myUnits.begin(); i != myUnits.end(); i++)
	{
		if (unitTypeCounts.find((*i)->getType()) == unitTypeCounts.end())
		{
			unitTypeCounts.insert(std::make_pair((*i)->getType(),0));
		}
		unitTypeCounts.find((*i)->getType())->second++;
	}

	int line = 1;
	for(std::map<UnitType,int>::iterator i = unitTypeCounts.begin(); i != unitTypeCounts.end(); i++)
	{
		Broodwar->drawTextScreen(5,16*line,"- %d %s",(*i).second, (*i).first.getName().c_str());
		line++;
	}

	std::set<Unit*> enemyUnits;
	for(std::set<BWAPI::Player*>::const_iterator i = BWAPI::Broodwar->enemies().begin(); i != BWAPI::Broodwar->enemies().end(); i++)
	{
		for(std::set<BWAPI::Unit*>::const_iterator u = (*i)->getUnits().begin(); u != (*i)->getUnits().end(); u++)
		{
			enemyUnits.insert(*u);
		}
	}

	Broodwar->drawTextScreen(160,0,"enemy %d units:", enemyUnits.size());
	std::map<UnitType, int> enemyUnitTypeCounts;

	for(std::set<Unit*>::iterator i = enemyUnits.begin(); i != enemyUnits.end(); i++)
	{
		if (enemyUnitTypeCounts.find((*i)->getType()) == enemyUnitTypeCounts.end())
		{
			enemyUnitTypeCounts.insert(std::make_pair((*i)->getType(),0));
		}
		enemyUnitTypeCounts.find((*i)->getType())->second++;
	}

	line = 1;
	for(std::map<UnitType,int>::iterator i = enemyUnitTypeCounts.begin(); i != enemyUnitTypeCounts.end(); i++)
	{
		Broodwar->drawTextScreen(160,16*line,"- %d %s",(*i).second, (*i).first.getName().c_str());
		line++;
	}

	//draw bullets
	std::set<Bullet*> bullets = Broodwar->getBullets();
	for(std::set<Bullet*>::iterator i = bullets.begin(); i != bullets.end(); i++)
	{
		Position p = (*i)->getPosition();
		double velocityX = (*i)->getVelocityX();
		double velocityY = (*i)->getVelocityY();
		if ((*i)->getPlayer() == Broodwar->self())
		{
			//Broodwar->drawLineMap(p.x(),p.y(),p.x()+(int)velocityX,p.y()+(int)velocityY,Colors::Green);
			//Broodwar->drawTextMap(p.x(),p.y(),"\x07%s",(*i)->getType().getName().c_str());
		}
		else
		{
			// Broodwar->drawLineMap(p.x(),p.y(),p.x()+(int)velocityX,p.y()+(int)velocityY,Colors::Red);
			//Broodwar->drawTextMap(p.x(),p.y(),"\x06%s",(*i)->getType().getName().c_str());
		}
	}

	//draw visibility data
	for(int x = 1; x<Broodwar->mapWidth(); x++)
	{
		for(int y = 1; y<Broodwar->mapHeight(); y++)
		{
			if (Broodwar->isExplored(x,y))
			{
				if (Broodwar->isVisible(x,y))
					Broodwar->drawDotMap(x*32,y*32,Colors::Green);
				else
					Broodwar->drawDotMap(x*32,y*32,Colors::Blue);
			}
			else
				Broodwar->drawDotMap(x*32,y*32,Colors::Red);
		}
	}

	//draw terrain data
	//we will iterate through all the base locations, and draw their outlines.
	for(std::set<BWTA::BaseLocation*>::const_iterator i = BWTA::getBaseLocations().begin(); i != BWTA::getBaseLocations().end(); i++)
	{
		TilePosition p = (*i)->getTilePosition();
		Position c = (*i)->getPosition();

		//draw outline of center location
		Broodwar->drawBox(CoordinateType::Map,p.x()*32,p.y()*32,p.x()*32+4*32,p.y()*32+3*32,Colors::Purple,false);

		//draw a outline at each mineral patch
		for(std::set<BWAPI::Unit*>::const_iterator j = (*i)->getStaticMinerals().begin(); j != (*i)->getStaticMinerals().end(); j++)
		{
			TilePosition q = (*j)->getInitialTilePosition();
			Broodwar->drawBox(CoordinateType::Map,q.x()*32,q.y()*32,q.x()*32+2*32,q.y()*32+1*32,Colors::Cyan,false);

		}

		//draw the outlines of vespene geysers
		for(std::set<BWAPI::Unit*>::const_iterator j = (*i)->getGeysers().begin(); j != (*i)->getGeysers().end(); j++)
		{
			TilePosition q = (*j)->getInitialTilePosition();
			Broodwar->drawBox(CoordinateType::Map,q.x()*32,q.y()*32,q.x()*32+4*32,q.y()*32+2*32,Colors::Orange,false);
		}

		//if this is an island expansion, draw a yellow circle around the base location
		if ((*i)->isIsland())
			Broodwar->drawCircle(CoordinateType::Map,c.x(),c.y(),80,Colors::Yellow,false);
	}

	//we will iterate through all the regions and draw the polygon outline of it in green.
	for(std::set<BWTA::Region*>::const_iterator r = BWTA::getRegions().begin(); r != BWTA::getRegions().end(); r++)
	{
		BWTA::Polygon p = (*r)->getPolygon();
		for(int j = 0; j < (int)p.size(); j++)
		{
			Position point1 = p[j];
			Position point2 = p[(j+1) % p.size()];
			Broodwar->drawLine(CoordinateType::Map,point1.x(),point1.y(),point2.x(),point2.y(),Colors::Green);
		}
	}

	//we will visualize the chokepoints with red lines
	for(std::set<BWTA::Region*>::const_iterator r = BWTA::getRegions().begin(); r != BWTA::getRegions().end(); r++)
	{
		for(std::set<BWTA::Chokepoint*>::const_iterator c = (*r)->getChokepoints().begin(); c != (*r)->getChokepoints().end(); c++)
		{
			Position point1 = (*c)->getSides().first;
			Position point2 = (*c)->getSides().second;
			Broodwar->drawLine(CoordinateType::Map,point1.x(),point1.y(),point2.x(),point2.y(),Colors::Red);
		}
	}
	

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//preprocessing

	bool flag[ MAX_ID ];// flag means whether the units exists or not.
	int counts_task[ task_end ];
	int nresource_depot = 0;
	int nmarine         = 0;
	int mineral_reserve = 0;
	int gas_reserve     = 0;
	int mineral_left    = Broodwar->self()->minerals();
	int gas_left        = Broodwar->self()->gas();
	int supply_total    = Broodwar->self()->supplyTotal()/2;
	int supply_used     = Broodwar->self()->supplyUsed()/2;
	Unit *resource_depot[ MAX_RESOURCE_DEPOT ];

	for ( int id = 0; id < MAX_ID; id += 1 ) { flag[id] = false; }
	for ( int i = 0; i < task_end; i += 1 )  { counts_task[i] = 0; }

	
	// set flag[]
	for(std::set<Unit*>::const_iterator i=Broodwar->self()->getUnits().begin(); i!=Broodwar->self()->getUnits().end(); i++)
	{
		int id = (*i)->getID();
		if ( MAX_ID <= id ) { Broodwar->printf( "INTERNAL ERROR! %s %d", __FILE__, __LINE__ ); }
		if ( flag[id] )        { Broodwar->printf( "INTERNAL ERROR! %s %d", __FILE__, __LINE__ ); }

		flag[id] = true;
	}

	// Set task_na if the unit of id doesn't exist, task_idle if exists.
	for ( int id = 0; id < MAX_ID; id += 1 )
	{
		if ( flag[id] )
		{
			if ( table_task[id].order == task_na ) { table_task[id].order = task_idle; }
		}
		else { table_task[id].order = task_na; }

		if ( table_task[id].order < 0 )         { Broodwar->printf( "INTERNAL ERROR! %s %d", __FILE__, __LINE__ ); }
		if ( task_end <= table_task[id].order ) { Broodwar->printf( "INTERNAL ERROR! %s %d", __FILE__, __LINE__ ); }
		counts_task[ table_task[id].order ] += 1;
	}

	int nsupply_depot = 0;
	int ncommand_center = 0;
	int nbarracks = 0;
	int num_scv = 0;

	// count building and set position
	for( std::set<Unit*>::const_iterator i = Broodwar->self()->getUnits().begin(); i != Broodwar->self()->getUnits().end(); i++ )
	{
		if( (*i)->isBeingConstructed()) continue;

		if ( (*i)->getType() == UnitTypes::Terran_Supply_Depot )
		{

			places_supply_depot[ nsupply_depot++ ] = (*i)->getPosition() - Position( UnitTypes::Terran_Supply_Depot.tileWidth()  * 16, 
				UnitTypes::Terran_Supply_Depot.tileHeight() * 16 );
		}

		else if ( (*i)->getType() == UnitTypes::Terran_Command_Center )
		{

			places_command_center[ ncommand_center++ ] = (*i)->getPosition() - Position( UnitTypes::Terran_Command_Center.tileWidth()  * 16, 
				UnitTypes::Terran_Command_Center.tileHeight() * 16 );
		}

		else if ( (*i)->getType() == UnitTypes::Terran_Barracks )
		{

			places_barracks[ nbarracks++ ] = (*i)->getPosition() - Position( UnitTypes::Terran_Barracks.tileWidth()  * 16, 
				UnitTypes::Terran_Barracks.tileHeight() * 16 );
		}
		else if( (*i)->getType() == UnitTypes::Terran_SCV )
		{
			num_scv++;
		}
	}


	//check whether first base was destroyed or not
	if( first_base_position == Positions::Invalid )
	{
		if ( ncommand_center )
		{
			first_base_position = places_command_center[0];
		}
	}
	else if ( ! first_base_destroy )
	{
		int cc;
		for ( cc = 0; cc < ncommand_center; cc++ )
		{
			if ( first_base_position == places_command_center[ cc ] ) { break; }
		}

		if ( cc == ncommand_center ) {
			first_base_destroy = true;
			//Broodwar->printf( "The first base is destroyed by enemies!" );
		}
	}

	//cancel task_building if SCV complete it 
	for(std::set<Unit*>::const_iterator i = Broodwar->self()->getUnits().begin(); i != Broodwar->self()->getUnits().end(); i++)
	{
		if ( (*i)->getType() != UnitTypes::Terran_SCV ) continue;

		int id = (*i)->getID();

		counts_task[ table_task[id].order ] -= 1;

		switch (  table_task[id].order )
		{
		case task_build_supply_depot:
			for ( int j = 0; j < nsupply_depot; j += 1 )
			{
				if ( table_task[id].position_target == places_supply_depot[j] ) table_task[id].order = task_mine_mineral;
			}
			break;
		case task_build_command_center:
			for ( int j = 0; j < ncommand_center; j += 1 )
			{
				if ( table_task[id].position_target == places_command_center[j] ) table_task[id].order = task_mine_mineral;
			}
			break;
		case task_build_barracks:
			for ( int j = 0; j < nbarracks; j += 1 )
			{
				if ( table_task[id].position_target == places_barracks[j] ) table_task[id].order = task_mine_mineral;
			}
			break;
		default:
			break;
		} 

		counts_task[ table_task[id].order ] += 1;
	}
	

	/* 
	ofstream visible_map("visible_map.txt");
	visible_map << Broodwar->getFrameCount() << endl;			
	for(int y = 0; y < Broodwar->mapHeight(); y++)
	{
		for(int x = 0; x < Broodwar->mapWidth(); x++)
		{
			if(Broodwar->isVisible(TilePosition(x, y)))
				visible_map << "1";
			else
				visible_map << "0";
		}
		visible_map << endl;
	}


	ofstream explored_map("explored_map.txt");
	explored_map << Broodwar->getFrameCount() << endl;			
	for(int y = 0; y < Broodwar->mapHeight(); y++)
	{
		for(int x = 0; x < Broodwar->mapWidth(); x++)
		{
			if(Broodwar->isExplored(TilePosition(x, y)))
				explored_map << "1";
			else
				explored_map << "0";
		}
		explored_map << endl;
	} 
	*/ 


	//decide isEnemyBase value
	int cnt = 0;
	for(std::set<BWTA::BaseLocation*>::const_iterator b = BWTA::getBaseLocations().begin(); b != BWTA::getBaseLocations().end(); b++,cnt++)
	{
		bool all_visible = true;
		int startx = (*b)->getTilePosition().x();
		int starty = (*b)->getTilePosition().y();

		for(int x = startx-1; x < startx + 5; x++ )
		{
			if(x < 0 || x >= map_width) continue;
			for(int y = starty-1; y < starty + 4; y++)
			{
				if(y < 0 || y >= map_height) continue;
				if(!Broodwar->isVisible(x, y))
				{
					all_visible = false;
					goto LABEL1;
				}
			}
		}
LABEL1:
		if(all_visible)
		{
			bool base_exist = false;
			for(std::set<BWAPI::Player*>::const_iterator i = BWAPI::Broodwar->enemies().begin(); i != BWAPI::Broodwar->enemies().end(); i++)
			{
				for(std::set<BWAPI::Unit*>::const_iterator u = (*i)->getUnits().begin(); u != (*i)->getUnits().end(); u++)
				{
					if ( (*u)->getType().isResourceDepot() )
					{
						if ( (((*b)->getPosition().x() - (*u)->getPosition().x())*((*b)->getPosition().x() - (*u)->getPosition().x()) +
							((*b)->getPosition().y() - (*u)->getPosition().y())*((*b)->getPosition().y() - (*u)->getPosition().y())) < 128*128
							)
						{
							base_exist = true;
							goto LABEL2;
						}
					}
				}
			}
LABEL2:
			if (!base_exist)
			{
				table_scout_history[cnt].isEnemyBase = false;
			}
		}
	}

	//enemies unit
	for(std::set<BWAPI::Player*>::const_iterator i = BWAPI::Broodwar->enemies().begin(); i != BWAPI::Broodwar->enemies().end(); i++)
	{
		for(std::set<BWAPI::Unit*>::const_iterator u = (*i)->getUnits().begin(); u != (*i)->getUnits().end(); u++)
		{

			Broodwar->drawTextMap((*u)->getPosition().x(),(*u)->getPosition().y(), "\x1D%s(%d)[%s]",
				(*u)->getType().getName().c_str(),
				(*u)->getID(),
				(*u)->getOrder().getName().c_str() );


			if ( (*u)->getType().isBuilding())
			{
				int closest_distance2 = INT_MAX;
				int closest_cnt       = -1;
				int found_x           = (*u)->getPosition().x();
				int found_y           = (*u)->getPosition().y();


				//Broodwar->printf("enemy_base : %d %d", (*u)->getPosition().x(), (*u)->getPosition().y() );

				for( int cnt = 0 ; cnt < nscout_history ; cnt++)
				{
					//Broodwar->printf("%d %d", table_scout_history[cnt].enemy_pos.x(), table_scout_history[cnt].enemy_pos.y());
					int base_x = table_scout_history[cnt].enemy_pos.x();
					int base_y = table_scout_history[cnt].enemy_pos.y();
					int dx = base_x - found_x;
					int dy = base_y - found_y;
					int distance2 = dx*dx + dy*dy;

					if ( distance2 < closest_distance2 )
					{
						closest_distance2 = distance2;
						closest_cnt = cnt;
						if ( distance2 < 128*128)
							break;
					}
				}
				if ( closest_distance2 < (16*32)*(16*32) )
				{
					table_scout_history[closest_cnt].isEnemyBase = true;
					//Broodwar->printf("Base found at: cnt=%d, x=%d y=%d", closest_cnt, table_scout_history[closest_cnt].enemy_pos.x(), table_scout_history[closest_cnt].enemy_pos.y());
				}
			}

		}
	}


	//Broodwar->drawTextScreen(300, 80, "base list");
	//set table_scout_history[].nFrame
	for( int cnt = 0 ; cnt < nscout_history ; cnt++)
	{
		//Broodwar->drawTextScreen(300,100+cnt*5, "%d %d", cnt, table_scout_history[cnt].nFrame );
			
		int startx = table_scout_history[cnt].enemy_pos.x()/32 - 2;
		int starty = table_scout_history[cnt].enemy_pos.y()/32 - 1;
		bool allvisible = true;

		for(int x = startx-1; x < startx+5; x++)
		{
			if(x < 0 || x >= map_width) continue;
			for(int y = starty-1; y < starty+4; y++)
			{
				if(y < 0 || y >= map_height) continue;

				if (!Broodwar->isVisible(x,y) )
				{
					allvisible = false;
					goto LABEL;
				}
			}
		}
LABEL:
		if(allvisible) 
		{
			table_scout_history[cnt].nFrame = Broodwar->getFrameCount();
		}
	}	

	// base statics information
	int nbase = 0;
	for( std::set<BWTA::BaseLocation*>::const_iterator b = BWTA::getBaseLocations().begin(); b != BWTA::getBaseLocations().end(); b++ )
	{
		bool flag = false;
		int x2 = Position((*b)->getTilePosition()).x();
		int y2 = Position((*b)->getTilePosition()).y();
		for (int i = 0; i < ncommand_center; i++)
		{
			int x1 = places_command_center[i].x();
			int y1 = places_command_center[i].y();

			int dx = x1 - x2;
			int dy = y1 - y2;
			if( dx*dx + dy*dy < 320*320)
			{
				flag = true;
				break;
			}				
		}

		if (!flag) continue;
		table_base[nbase].num_scv = 0;
		table_base[nbase].num_patch = 0;
		table_base[nbase].pos = (*b)->getPosition();

		nbase++;	
	}
 
	bool needBalanceSCV     = false;
	int  total_mineral_patch = 0;
	int  total_mining_scv   = 0;
	for (int i = 0; i < nbase; i++)
	{
		int x1, y1;
		x1 = table_base[i].pos.x();
		y1 = table_base[i].pos.y();
		for ( std::set<Unit*>::const_iterator m = Broodwar->getMinerals().begin(); m != Broodwar->getMinerals().end(); m++ )
		{
			int x2, y2;
			x2 = (*m)->getPosition().x();
			y2 = (*m)->getPosition().y();

			if ( (x1-x2)*(x1-x2) + (y1-y2)*(y1-y2) <= 320*320 )
			{
				table_base[i].num_patch++;
			}
		}
		//Broodwar->drawTextMap(table_base[i].pos.x()-16, table_base[i].pos.y(), "\x17 patch:%d", table_base[i].num_patch );


		for( std::set<Unit*>::const_iterator w = Broodwar->self()->getUnits().begin(); w != Broodwar->self()->getUnits().end(); w++ )
		{
			if (!(*w)->getType().isWorker()) continue;
			int id = (*w)->getID();
			if (table_task[id].order != task_mine_mineral) continue; 

			int x2, y2;
			x2 = (*w)->getPosition().x();
			y2 = (*w)->getPosition().y();

			if ( (x1-x2)*(x1-x2) + (y1-y2)*(y1-y2) <= 320*320 )
			{
				table_base[i].num_scv++;
			}
		} 
		//Broodwar->drawTextMap(table_base[i].pos.x()-16, table_base[i].pos.y()+16, "\x17 scv:%d", table_base[i].num_scv ); 

		//total_mineral_patch += table_base[i].num_patch;
		//total_mining_scv    += table_base[i].num_scv;
	}
	//Broodwar->printf("total_mineral_patch*3 %d", total_mineral_patch*3);
	//Broodwar->printf("total_mining_scv      %d", total_mining_scv);

	Unit* mineral_to        = NULL;
	Position CC_SCV_MAX_pos = Positions::Invalid;

	float min_ratio = 1e5;
	float max_ratio = 0.0;
	Position CC_SCV_MIN_pos = Positions::Invalid;

	for( int ibase = 0; ibase < nbase; ibase++ )
	{
		float ratio;

		if ( table_base[ibase].num_patch == 0 ) { ratio = 1e10; }
		else {
			ratio = (float)table_base[ibase].num_scv / (float)table_base[ibase].num_patch;
		}

		if ( ratio < min_ratio )
		{
			min_ratio = ratio;
			CC_SCV_MIN_pos = table_base[ibase].pos;
		}

		if ( max_ratio < ratio )
		{
			max_ratio = ratio;
			CC_SCV_MAX_pos = table_base[ibase].pos;
		}
	}

	//Broodwar->printf("max_ratio %f", max_ratio );
	//Broodwar->printf("min_ratio %f", min_ratio );

	if(CC_SCV_MIN_pos != Positions::Invalid)
	{
		for ( std::set<Unit*>::const_iterator m = Broodwar->getMinerals().begin(); m != Broodwar->getMinerals().end(); m++ )
		{
			if((*m)->getDistance(CC_SCV_MIN_pos) < 32*10)
			{
				mineral_to = (*m);
				break;
			}
		}
	}

	if ( 2.5 < max_ratio && min_ratio < 2.0 )
	{
		needBalanceSCV = true;
		//Broodwar->printf("needBalanceSCV becomes true");
	}

	/*
	int num_producing_building = 0;
	for( std::set<Unit*>::const_iterator i = Broodwar->self()->getUnits().begin(); i != Broodwar->self()->getUnits().end(); i++ )
	{
		if((*i)->getType() != UnitTypes::Terran_Command_Center 
		&& (*i)->getType() != UnitTypes::Terran_Barracks) continue;
		num_producing_building++;
	}
	*/ 

	for( std::set<Unit*>::const_iterator u = Broodwar->self()->getUnits().begin(); u != Broodwar->self()->getUnits().end(); u++ )
	{
		if((*u)->getType() != UnitTypes::Terran_Marine) continue;
		int id = (*u)->getID();
		marine_data[id].friend_power = 0;
		marine_data[id].enemy_power = 0;
	}


	for( std::set<Unit*>::const_iterator su1 = Broodwar->self()->getUnits().begin(); su1 != Broodwar->self()->getUnits().end(); su1++ )
	{
		if((*su1)->getType() != UnitTypes::Terran_Marine) continue;
		int id = (*su1)->getID();
		for( std::set<Unit*>::const_iterator su2 = Broodwar->self()->getUnits().begin(); su2 != Broodwar->self()->getUnits().end(); su2++ )
		{
			if((*su2)->getType() != UnitTypes::Terran_Marine) continue;
			if((*su1)->getID() == (*su2)->getID()) continue;
			if((*su1)->getDistance(*su2) < 15*32 ) 
				marine_data[id].friend_power++;
		}

		marine_data[id].escape = false;

		for(std::set<BWAPI::Player*>::const_iterator ep = BWAPI::Broodwar->enemies().begin(); ep != BWAPI::Broodwar->enemies().end(); ep++)
		{
			for(std::set<BWAPI::Unit*>::const_iterator eu = (*ep)->getUnits().begin(); eu != (*ep)->getUnits().end(); eu++)
			{
				if((*su1)->getDistance(*eu) < 15*32)
					marine_data[id].enemy_power++;
			}
		}

		//Broodwar->printf("friend power  %d", marine_data[id].friend_power);
		//Broodwar->printf("enemy power   %d", marine_data[id].enemy_power);

		if(marine_data[id].friend_power <= marine_data[id].enemy_power) marine_data[id].escape = true;
	}

	/*
	int FrameCount = 1000;
	if(Broodwar->getFrameCount() == FrameCount)
	{
		Broodwar->leaveGame();
	}
	*/ 
	 

	/*std::ofstream resource_data("resource_data.txt", std::ios::out);

	if ( BWAPI::Broodwar->getFrameCount() % 1000 == 0 )
	{
		BWAPI::Broodwar->printf("%d %d %d" ,BWAPI::Broodwar->getFrameCount(), BWAPI::Broodwar->self()->gatheredMinerals(), BWAPI::Broodwar->self()->gatheredGas() );
		resource_data << BWAPI::Broodwar->getFrameCount() << " "  <<  BWAPI::Broodwar->self()->gatheredMinerals() << " " << BWAPI::Broodwar->self()->gatheredGas() << std::endl;
	}*/ 

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// assign task
	for( std::set<Unit*>::const_iterator i = Broodwar->self()->getUnits().begin(); i != Broodwar->self()->getUnits().end(); i++ )
	{
		bool refinary_exist = false;

		//check error
		int id = (*i)->getID();
		if ( table_task[id].order == task_na )       { Broodwar->printf( "INTERNAL ERROR! %s %d", __FILE__, __LINE__ ); }
		if ( ! counts_task[ table_task[id].order ] ) { Broodwar->printf( "INTERNAL ERROR! %s %d", __FILE__, __LINE__ ); }

		counts_task[ table_task[id].order ] -= 1;


		// Command Center
		if(  (*i)->getType() == UnitTypes::Terran_Command_Center )
		{
			resource_depot[ nresource_depot++ ] = *i;
			if ( table_task[id].order == task_idle  && num_scv <= 100 ) 
			{
				table_task[id].order = task_train_scv;
				table_task[id].position_target = Positions::Invalid;
				table_task[id].id_target = -1;
			}
		}

		//Refinery
	    else if ((*i)->getType() == UnitTypes::Terran_Refinery )
		{
			refinary_exist = true;
		}

		//Baracks
		 else if ( (*i)->getType() == UnitTypes::Terran_Barracks )
		{
			if ( table_task[id].order == task_idle )
			{
				table_task[id].order = task_set_rallypoint;
				table_task[id].position_target = Positions::Invalid;
				table_task[id].id_target = -1;
			}
			else if ( table_task[id].order != task_train_marine )
			{
				table_task[id].order = task_train_marine;
				table_task[id].position_target = Positions::Invalid;
				table_task[id].id_target = -1;
			}
		}

		//Factory
		 /*else if ( (*i)->getType() == UnitTypes::Terran_Factory )
		 {
			if ( table_task[id].order == task_idle )
			{
					table_task[id].order = task_build_machine_shop;
					table_task[id].position_target = Positions::Invalid;
					table_task[id].id_target = -1;
			}
			else if ( table_task[id].order != task_train_tank &&
				      counts_task[task_build_machine_shop] > 0)
			{
					table_task[id].order = task_train_tank;
					table_task[id].position_target = Positions::Invalid;
					table_task[id].id_target = -1;
			}
		 }*/

		//SCV 
		else if ( (*i)->getType().isWorker() )
		{
			Position c=(*i)->getPosition();
			switch ( table_task[id].order )
			{
			case task_idle:
			case task_mine_mineral:
				if ( (*i)->getOrder() == Orders::Nothing ) // SCV inside of a command center + Strange SCV
				{

				}
				else if ((*i)->getOrder() == Orders::PlayerGuard)
				{
					table_task[id].order = task_mine_mineral;
					table_task[id].position_target = Positions::Invalid;
					table_task[id].id_target = -1;
				}
				else if ( counts_task[ task_mine_mineral] >= 8 && !counts_task[ task_scout] )
				{
					table_task[id].order = task_scout;
					table_task[id].position_target = Positions::Invalid;
					table_task[id].id_target = -1;
				} 
				else if (  !counts_task[ task_build_supply_depot ] &&  mineral_left >= 100 && ncommand_center*10 + nsupply_depot*8 < 250)
				{
					int margine = 0;
					if(counts_task[task_mine_mineral] < 16)  margine = 3;
					else if(counts_task[task_mine_mineral] < 32) margine = 6;
					else if(counts_task[task_mine_mineral] < 48) margine = 9;
					else margine = 12;
					if(supply_total - supply_used <= margine)
					{
						table_task[id].order = task_build_supply_depot;
						table_task[id].position_target = Positions::Invalid;
						table_task[id].id_target = -1;
					}
				}
				else if ( !counts_task[ task_build_barracks ]  
				&&  supply_used > 11 
					)
				{	
					table_task[id].order = task_build_barracks;
					table_task[id].position_target = Positions::Invalid;
					table_task[id].id_target = -1;
				}
				else if (  !counts_task[ task_build_command_center ]  
						&& mineral_left >= 400 - counts_task[task_mine_mineral] * 16
						&& (   ( 16 < counts_task[ task_mine_mineral ] && ncommand_center < 2 )
							|| ( 32 < counts_task[ task_mine_mineral ] && ncommand_center < 3 )
							|| ( 48 < counts_task[ task_mine_mineral ] ) 
							)
						)
				{   
					table_task[id].order = task_build_command_center;
					table_task[id].position_target = Positions::Invalid;
					table_task[id].id_target = -1;
				}
				/*else if (! counts_task[task_build_refinery] 
				&& counts_task[task_build_barracks] > 0)
				{
					table_task[id].order = task_build_refinery;
					table_task[id].position_target = Positions::Invalid;
					table_task[id].id_target = -1;
				}*/ 
				/*else if ( refinary_exist && counts_task[task_collect_gas] < 3 )
				{
					table_task[id].order = task_collect_gas;
					table_task[id].position_target = Positions::Invalid;
					table_task[id].id_target = -1;
				}*/
				/*else if ( !counts_task[task_build_factory]
				&& counts_task[task_build_barracks] > 0)
				{
					table_task[id].order = task_build_factory;
					table_task[id].position_target = Positions::Invalid;
					table_task[id].id_target = -1;
				}*/
				else {
					table_task[id].order = task_mine_mineral;
					table_task[id].position_target = Positions::Invalid;
					table_task[id].id_target = -1;
				}	
				break;/* 
			case task_build_supply_depot:
				for ( int i = 128; i < 1024*2; i += 128 )
				{
					Broodwar->drawCircle(CoordinateType::Map,c.x(),c.y(),i,Colors::Red,false);
				}
				break;
			case task_build_command_center:
				for ( int i = 128; i < 1024*2; i += 128 )
				{
					Broodwar->drawCircle(CoordinateType::Map,c.x(),c.y(),i,Colors::Yellow,false);
				}
				break;
			case task_build_refinery:
				for ( int i = 128; i < 1024*2; i += 128 )
				{
					Broodwar->drawCircle(CoordinateType::Map, c.x(), c.y(), i, Colors::Orange, false);
				}
				break;
			case task_build_barracks:
				for ( int i = 128; i < 1024*2; i += 128 )
				{
					Broodwar->drawCircle(CoordinateType::Map,c.x(),c.y(),i,Colors::Blue,false);
				}
				break;
			case task_scout:
				for ( int i = 128; i < 1024*2; i += 128 )
				{
					Broodwar->drawCircle(CoordinateType::Map,c.x(),c.y(),i,Colors::Cyan,false);
				}
				break;*/ 
			}
		} 

		//Marine
		else if ( (*i)->getType() == UnitTypes::Terran_Marine )
		{
			nmarine++;

			if ( (*i)->getOrder() == Orders::Nothing )
			{
				table_task[id].order = task_idle;
				table_task[id].position_target = Positions::Invalid;
				table_task[id].id_target = -1;
			}
			else if(table_task[id].order != task_attack)
			{
				table_task[id].order = task_attack;
				table_task[id].position_target = Positions::Invalid;
				table_task[id].id_target = -1;
			}
			else if (table_task[id].order != task_escape && marine_data[id].escape)
			{	
				table_task[id].order = task_escape;
				table_task[id].position_target = Positions::Invalid;
				table_task[id].id_target = -1;
			} 
			else if(table_task[id].order != task_attack)
			{
				table_task[id].order = task_attack;
				table_task[id].position_target = Positions::Invalid;
				table_task[id].id_target = -1;
			} 
		}
		else 
		{
		}
		counts_task[ table_task[id].order ] += 1;
	}

	// Check error and Count mineral_reserve
	for( std::set<Unit*>::const_iterator i = Broodwar->self()->getUnits().begin(); i != Broodwar->self()->getUnits().end(); i++ )
	{
		int id = (*i)->getID();
		if ( table_task[id].order == task_na )  { Broodwar->printf( "INTERNAL ERROR! %s %d", __FILE__, __LINE__ ); }
		if ( table_task[id].order < 0 )         { Broodwar->printf( "INTERNAL ERROR! %s %d", __FILE__, __LINE__ ); }
		if ( task_end <= table_task[id].order ) { Broodwar->printf( "INTERNAL ERROR! %s %d", __FILE__, __LINE__ ); }

		if ( table_task[id].order == task_build_supply_depot   && (*i)->getOrder() != Orders::ConstructingBuilding ) mineral_reserve += 100; 	
		if ( table_task[id].order == task_build_command_center && (*i)->getOrder() != Orders::ConstructingBuilding ) mineral_reserve += 400; 
		if ( table_task[id].order == task_build_barracks && (*i)->getOrder() != Orders::ConstructingBuilding ) mineral_reserve += 100; 
		//if ( table_task[id].order == task_build_refinery && (*i)->getOrder() != Orders::ConstructingBuilding ) mineral_reserve += 100; 
		//if ( table_task[id].order == task_build_factory && (*i)->getOrder() != Orders::ConstructingBuilding ) { mineral_reserve += 200, gas_reserve += 100; } 
	} 

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	//execute task
	for( std::set<Unit*>::const_iterator i = Broodwar->self()->getUnits().begin(); i != Broodwar->self()->getUnits().end(); i++ )
	{
		int id = (*i)->getID();

		if ( table_task[id].order == task_na ) { Broodwar->printf( "INTERNAL ERROR! %s %d", __FILE__, __LINE__ ); }

		if ( table_task[id].order == task_train_scv ) 
		{
			if ( ! (*i)->isTraining() && mineral_left >= 50 + mineral_reserve ) { (*i)->train(UnitTypes::Terran_SCV); }
		}

		if ( table_task[id].order == task_train_marine )
		{
			Broodwar->drawTextMap((*i)->getPosition().x(),(*i)->getPosition().y()-16,
				"\x11%d %d", (*i)->isTraining(), mineral_reserve);
			if ( ! (*i)->isTraining() && mineral_left >= 50 + mineral_reserve ) { (*i)->train(UnitTypes::Terran_Marine);}
		}

		if ( table_task[id].order == task_set_rallypoint )
		{
			if ( (*i)->getType() == UnitTypes::Terran_Barracks )
			{
				Position closestChokepoint = Positions::Invalid;
				int closest_distance = INT_MAX;
				for(std::set<BWTA::Region*>::const_iterator r=BWTA::getRegions().begin();r!=BWTA::getRegions().end();r++)
				{
					for(std::set<BWTA::Chokepoint*>::const_iterator c=(*r)->getChokepoints().begin();c!=(*r)->getChokepoints().end();c++)
					{
						Position point1 = (*c)->getSides().first;
						Position point2 = (*c)->getSides().second;
						Position point3 =  Position((point1.x()+point2.x())/2, (point1.y()+point2.y())/2);

						if ((*i)->getDistance(point3) < closest_distance)
						{
							closestChokepoint = point3;
							closest_distance = (*i)->getDistance(point3);
						}
					}
				}
				if ( closestChokepoint != Positions::Invalid )
				{
					(*i)->setRallyPoint(closestChokepoint);
				}
			}
		}

		if ( table_task[id].order == task_mine_mineral )
		{
			if(needBalanceSCV && nresource_depot > 1 && mineral_to != NULL && (*i)->getDistance(CC_SCV_MAX_pos) < 320 )
			{
				(*i)->rightClick(mineral_to);
				needBalanceSCV = false;
			}
			else if ( (*i)->getOrder() == Orders::PlayerGuard )
			{
				Unit *closestMineral = NULL;
				int closest_distance = INT_MAX;
				for( std::set<Unit*>::const_iterator m = Broodwar->getMinerals().begin(); m != Broodwar->getMinerals().end(); m++ )
				{
					if ( (*i)->getDistance(*m) < closest_distance )
					{
						closestMineral   = *m;
						closest_distance = (*i)->getDistance(*m);
					}
				}
				if ( closestMineral != NULL )
				{
					(*i)->rightClick(closestMineral);
					table_task[id].position_target = closestMineral->getPosition();
				}
			}   
		}

		if ( table_task[id].order == task_collect_gas )
		{
			Unit* u = *(getStartLocation(Broodwar->self())->getGeysers().begin());
			(*i)->gather(u);
		}

		if ( table_task[id].order == task_scout && (*i)->getOrder() != Orders::Move)
		{
			int min_score = INT_MAX;
			Position closest_baselocation = Positions::Invalid;
			for( int cnt = 0 ; cnt < nscout_history ; cnt++)
			{
				Position p   = table_scout_history[cnt].enemy_pos;
				int distance = (*i)->getDistance(p);

				if ( table_scout_history[cnt].isEnemyBase ) continue;
				if ( !(*i)->hasPath(p) ) continue;
				if ( distance < 32 ) continue;

				int score = distance + table_scout_history[cnt].nFrame;
				//Broodwar->drawTextMap( p.x(), p.y(), "score: %d = %d + %d", score, distance, table_scout_history[cnt].nFrame );
				if ( score < min_score )
				{
					min_score            = score;
					closest_baselocation = p;
				}
			}
			//Broodwar->printf( "min score: %d", min_score );

			if ( closest_baselocation != Positions::Invalid)
			{
				(*i)->rightClick(closest_baselocation);
			}
		}

		if ( table_task[id].order == task_attack )
		{
			int closest_distance = INT_MAX;
			Unit* closest_enemy = NULL;
			for(std::set<BWAPI::Player*>::const_iterator e = BWAPI::Broodwar->enemies().begin(); e != BWAPI::Broodwar->enemies().end(); e++)
			{
				for(std::set<BWAPI::Unit*>::const_iterator u = (*e)->getUnits().begin(); u != (*e)->getUnits().end(); u++)
				{
					if ( (*i)->getDistance(*u) < closest_distance )
					{
						closest_distance =  (*i)->getDistance(*u);
						closest_enemy = (*u);
					}
				}
			}

			/*if(closest_enemy != NULL)
			{
            (*i)->attack(closest_enemy);
			}*/
		     if ( closest_distance < 192)// 32 * 6 = 192
			{
				int id_target = closest_enemy->getID();
				//Broodwar->printf("enemy %s",closest_enemy->getType().getName().c_str());
				if ( id_target != table_task[id].id_target ) {
					table_task[id].id_target = id_target;
					table_task[id].position_target = Positions::Invalid;
					(*i)->attack(closest_enemy);

					Position self = (*i)->getPosition();
					Position enemy = closest_enemy->getPosition();
					Broodwar->drawLine(CoordinateType::Map,self.x(),self.y(),enemy.x(),enemy.y(),Colors::Orange);
				}
			}
			else if ( nmarine > 30 )
			{
				for(int ibase = 0; ibase < MAX_BASE; ibase++)
				{
					if ( table_scout_history[ibase].isEnemyBase )
					{	 
						Position position_target = table_scout_history[ibase].enemy_pos;						
						if (position_target != table_task[id].position_target
							)
						{		
							table_task[id].position_target = position_target;
							table_task[id].id_target = -1;
							(*i)->rightClick(position_target);
							if ((*i)->getOrder() == Orders::PlayerGuard)
							{
								int id_target = closest_enemy->getID();
								//Broodwar->printf("enemy %s",closest_enemy->getType().getName().c_str());
								if ( id_target != table_task[id].id_target )
								{
									table_task[id].id_target = id_target;
									table_task[id].position_target = Positions::Invalid;
									(*i)->attack(closest_enemy);
									Position self = (*i)->getPosition();
									Position enemy = closest_enemy->getPosition();
									Broodwar->drawLine(CoordinateType::Map,self.x(),self.y(),enemy.x(),enemy.y(),Colors::Orange);
								}
							}
						}
						Position p = (*i)->getPosition();
						Broodwar->drawCircle(CoordinateType::Map,p.x(),p.y(),32,Colors::Teal,false);
						break;
					}
				}
			}   
		}

		if(table_task[id].order == task_escape)
		{
			(*i)->move(first_base_position);
		}

		if( nresource_depot != 0
			&& mineral_left >= 100
			&& (*i)->getOrder() != Orders::PlaceBuilding
			&& (*i)->getOrder() != Orders::ConstructingBuilding
			&& table_task[id].order == task_build_supply_depot )
		{    
			TilePosition sd_place;
			struct cmp_buildable_work work;
			int execute_build_supply_depot;

			update_buildable();

			work.ptbl   = &table_buildable;
			work.width  = UnitTypes::Terran_Supply_Depot.tileWidth();
			work.height = UnitTypes::Terran_Supply_Depot.tileHeight();
			if( first_base_position != Positions::Invalid && ! first_base_destroy )
			{
				sd_place =	foo_spiral_search(TilePosition(first_base_position), 64, &work, cmp_buildable );
			}
			else
			{
				sd_place =	foo_spiral_search( resource_depot[0]->getTilePosition(), 64, &work, cmp_buildable );
			}
			if ( sd_place == TilePositions::Invalid )
			{
				Broodwar->printf( "Spiral search returns invalid.\n" );
			}
			else
			{
				(*i)->build(sd_place, UnitTypes::Terran_Supply_Depot);
				execute_build_supply_depot = Broodwar->getFrameCount();
			}
			table_task[id].position_target = Position(sd_place);
		}

		if( nresource_depot != 0
			&& (*i)->getOrder() != Orders::Move
			&& (*i)->getOrder() != Orders::PlaceBuilding
			&& (*i)->getOrder() != Orders::ConstructingBuilding
			&& table_task[id].order == task_build_command_center )
		{ 
			std::set<BWTA::BaseLocation*>::const_iterator b;
			bool arrived = false;
			for(b = BWTA::getBaseLocations().begin(); b != BWTA::getBaseLocations().end(); b++ )
			{
				int dx = (*i)->getPosition().x() - (*b)->getPosition().x();
				int dy = (*i)->getPosition().y() - (*b)->getPosition().y();
				if(dx * dx + dy * dy <= 3*32 * 3*32)
				{
					arrived = true;
					break;
				}
			}
			if(arrived)
			{
				if ( mineral_left >= 400 )
				{
					(*i)->build( (*b)->getTilePosition(), UnitTypes::Terran_Command_Center );
					table_task[id].position_target = Position((*b)->getTilePosition());
				}
			}
			else 
			{
				int closest_distance = INT_MAX;
				Position closest_location = Positions::Invalid;
			 
				for(int cnt = 0; cnt < nscout_history; cnt++)
				{	

					Position place = table_scout_history[cnt].enemy_pos;
					if( !Broodwar->hasPath( (*i)->getPosition(), place ) ) continue; 

					if(table_scout_history[cnt].isEnemyBase) continue;
					
					int rd_id;
					for( rd_id = 0; rd_id < nresource_depot; rd_id++)
					{
						if ( place == resource_depot[rd_id]->getPosition() ) break; 
					}
					if ( rd_id < nresource_depot ) 
					{
                       continue;
					}
                    

					for ( int rd_id = 0; rd_id < nresource_depot; rd_id++)
					{
						if ( resource_depot[rd_id]->getDistance(place) < closest_distance  )
						{
							closest_location  = place;
							closest_distance = resource_depot[rd_id]->getDistance(place);
						}	
					}
				}
				//Broodwar->printf("closest_distance %d", closest_distance);
				//Broodwar->printf("closest_location %d %d", closest_location->getPosition().x()/32, closest_location->getPosition().y()/32);

				if ( closest_location != Positions::Invalid)
				{
					Position p     = closest_location;
					Broodwar->drawCircle( CoordinateType::Map, p.x(), p.y(), 30, Colors::Purple, false );
					(*i)->rightClick(p);
				}
			}
		} 

		if( nresource_depot != 0
			&& mineral_left >= 150
			&& (*i)->getOrder() != Orders::PlaceBuilding
			&& (*i)->getOrder() != Orders::ConstructingBuilding
			&& table_task[id].order == task_build_barracks )
		{
			TilePosition bp;
			struct cmp_buildable_work work;
			update_buildable();
			work.ptbl   = &table_buildable;
			work.width  = UnitTypes::Terran_Barracks.tileWidth();
			work.height = UnitTypes::Terran_Barracks.tileHeight();
			bp =	foo_spiral_search( resource_depot[0]->getTilePosition(), 64, &work, cmp_buildable );
			(*i)->build(bp, UnitTypes::Terran_Barracks);
			table_task[id].position_target = Position(bp);
		}



		/*if(  nresource_depot != 0
			&& mineral_left >= 100
			&& (*i)->getOrder() != Orders::PlaceBuilding
			&& (*i)->getOrder() != Orders::ConstructingBuilding
			&& table_task[id].order == task_build_refinery )
		{
			int closest_distance = INT_MAX;
			Position closest_geyser = Positions::Invalid;

			for(std::set<BWTA::BaseLocation*>::const_iterator bl = BWTA::getBaseLocations().begin(); bl != BWTA::getBaseLocations().end(); bl++)
			{
				for(std::set<BWAPI::Unit*>::const_iterator g = (*bl)->getGeysers().begin(); g != (*bl)->getGeysers().end(); g++)
				{
					TilePosition gpt = (*g)->getInitialTilePosition();
					Position gp = Position(gpt);

					if ( (*i)->getDistance(gp) < closest_distance )
					{
						closest_geyser = gp;
						closest_distance = (*i)->getDistance(gp);
					}
				}
			}
			if (closest_geyser != Positions::Invalid)
			{
				//Broodwar->printf("gas pos %d %d", closest_geyser.x()/32, closest_geyser.y()/32);
				(*i)->build(TilePosition(closest_geyser), UnitTypes::Terran_Refinery);
			}
		}

		if(  nresource_depot != 0
			&& mineral_left >= 200
			&& gas_left >= 100
			&& (*i)->getOrder() != Orders::PlaceBuilding
			&& (*i)->getOrder() != Orders::ConstructingBuilding
			&& table_task[id].order == task_build_factory )
		{

			TilePosition ftp;
			struct cmp_buildable_work work;
			update_buildable();

			work.ptbl   = &table_buildable;
			work.width  = UnitTypes::Terran_Factory.tileWidth();
			work.height = UnitTypes::Terran_Factory.tileHeight();
			ftp =	foo_spiral_search( resource_depot[0]->getTilePosition(), 64, &work, cmp_buildable );
			(*i)->build(ftp, UnitTypes::Terran_Factory);
			table_task[id].position_target = Position(ftp);
		}
		if ( table_task[id].order == task_build_machine_shop &&
			nresource_depot != 0 &&
			mineral_left >= 50 &&
			gas_left >= 50 &&
			(*i)->getOrder() != Orders::PlaceBuilding &&
			(*i)->getOrder() != Orders::ConstructingBuilding )
		{
			(*i)->buildAddon(UnitTypes::Terran_Machine_Shop);
		}
		if ( table_task[id].order == task_train_tank &&
			nresource_depot > 0 &&
			mineral_left >= 150 + mineral_reserve &&
			gas_left >= 100 + gas_reserve &&
			(*i)->getOrder() != Orders::PlaceBuilding &&
			(*i)->getOrder() != Orders::ConstructingBuilding &&
			! (*i)->isTraining() )
		{
		    (*i)->train(UnitTypes::Terran_Marine);
		}*/
	}
}


void Oritaka::onSendText(std::string text)
{
	BWAPI::Broodwar->sendText(text.c_str());
}



void update_buildable()
{	
	for( int y = 0; y< map_height; y++ ) 
	{
		for( int x = 0; x < map_width; x++ ) 
		{
			table_buildable[x][y] = Broodwar->isBuildable(x, y);
		}
	}

	for( std::set<Unit*>::const_iterator m = Broodwar->getMinerals().begin(); m != Broodwar->getMinerals().end(); m++ ) 
	{
		for ( int x = 0; x < (*m)->getType().tileWidth(); x++ ) 
		{
			if ( map_width <= (*m)->getTilePosition().x() + x )  continue; 
			for ( int y = 0; y < (*m)->getType().tileHeight(); y++ ) 
			{
				if ( map_height <= (*m)->getTilePosition().y() + y )  continue; 
				table_buildable[(*m)->getTilePosition().x() + x ][(*m)->getTilePosition().y() + y] = false;
			}
		}
	}

	for( std::set<Unit*>::const_iterator g = Broodwar->getGeysers().begin(); g != Broodwar->getGeysers().end(); g++ ) 
	{
		for( int x = 0; x < (*g)->getType().tileWidth(); x++ ) 
		{
			if ( map_width <= (*g)->getTilePosition().x() + x )  continue; 
			for( int y = 0; y < (*g)->getType().tileHeight(); y++ ) 
			{
				if ( map_height <= (*g)->getTilePosition().y() + y )  continue; 
				table_buildable[(*g)->getTilePosition().x() + x ][(*g)->getTilePosition().y() + y] = false;
			}	
		}
	}

	for( std::set<Unit*>::const_iterator i=Broodwar->self()->getUnits().begin(); i != Broodwar->self()->getUnits().end(); i++ ) 
	{
		if ( (*i)->getType().isBuilding() ) 
		{
			//Terran_Command_Center
			if ( (*i)->getType() == UnitTypes::Terran_Command_Center ) 
			{
				Unit* cc = *i;

				for( std::set<Unit*>::const_iterator m = Broodwar->getMinerals().begin(); m != Broodwar->getMinerals().end(); m++ ) 
				{
					if( cc->getDistance((*m)) < 320 ) 
					{
						foo_bresenham( cc->getTilePosition().x() + cc->getType().tileWidth()/2,
							cc->getTilePosition().y() + cc->getType().tileHeight()/2,
							(*m)->getTilePosition().x() + (*m)->getType().tileWidth()/2,
							(*m)->getTilePosition().y() + (*m)->getType().tileHeight()/2,
							set_buildable );
					}
				}

				for( std::set<Unit*>::const_iterator g = Broodwar->getGeysers().begin(); g != Broodwar->getGeysers().end(); g++ ) 
				{
					if( cc->getDistance((*g)) < 320 ) 
					{
						foo_bresenham(  cc->getTilePosition().x() + cc->getType().tileWidth()/2,
							cc->getTilePosition().y() + cc->getType().tileHeight()/2,
							(*g)->getTilePosition().x() + (*g)->getType().tileWidth()/2,
							(*g)->getTilePosition().y() + (*g)->getType().tileHeight()/2,
							set_buildable);
					}
				}
			}

			//UnitTypes::Terran_Factory
			/*if((*i)->getType() == UnitTypes::Terran_Factory)
			{
				for(int x = 0; x < (*i)->getType().tileWidth()+3; x++)
				{
					if ( map_width <= (*i)->getTilePosition().x() + x )  continue; 
					for(int y = 0; y < (*i)->getType().tileHeight(); y++)
					{
						if ( map_height <= (*i)->getTilePosition().y() + y )  continue; 
						table_buildable[(*i)->getTilePosition().x() + x ][(*i)->getTilePosition().y() + y] = false;
					}
				}
			}*/

			//general building 
			for(int x=-1; x<(*i)->getType().tileWidth()+1; x++) 
			{
				if ( map_width <= (*i)->getTilePosition().x() + x )  continue; 
				for(int y=-1; y<(*i)->getType().tileHeight()+1; y++) 
				{
					if ( map_height <= (*i)->getTilePosition().y() + y )  continue; 
					table_buildable[(*i)->getTilePosition().x() + x ][(*i)->getTilePosition().y() + y] = false;

				}
			}
		}		
	}
}

void set_buildable( int x, int y )
{
	for ( int dx = -1 ; dx < 2 ; dx++){
		if ( x + dx <= 0 || map_width <= x + dx ) continue;
		for ( int dy = -1 ; dy < 2 ; dy++){
			if ( 0 <= y + dy && y + dy < map_height ) { 
				table_buildable[ x + dx ][ y + dy ] = false;
			}
		}
	}
}


void foo_bresenham( int x1, int y1, int x2, int y2, void (*foo)( int x, int y ) )
{
	int dx = abs(x2 - x1);
	int dy = abs(y2 - y1);
	int sx = x2 > x1 ? 1 : -1;
	int sy = y2 > y1 ? 1 : -1;

	if( dx > dy){
		int E = -dx;
		for( int j = 0; j <= dx; j++ ) {
			foo( x1, y1 );
			x1 += sx;
			E += 2 * dy;
			if( E >= 0 ) {
				y1 += sy;
				E -= 2 * dx;
			}
		}
	} else {
		int E = -dy;
		for( int j = 0; j <= dy; j++ ) {
			foo( x1, y1 );
			y1 += sy;
			E  += 2 * dx;
			if( E >= 0 ) {
				x1 += sx;
				E -= 2 * dy;
			}
		}
	}
}

bool cmp_buildable( int x, int y, struct cmp_buildable_work *p )
{
	int width  = p->width;
	int height = p->height;
	bool (*ptbl)[ MAX_MAP_WIDTH ][ MAX_MAP_HEIGHT ] = p->ptbl;

	for ( int dx = 0; dx < width; dx++ )
		for ( int dy = 0; dy < height; dy++ )
		{
			if( x+dx < 0 || x+dx >= map_width || y+dy < 0 || y+dy >= map_height) return false;
			if ( ! (*ptbl)[x+dx][y+dy] )  return false;	
		}

		return true;
}



BWAPI::TilePosition foo_spiral_search( const BWAPI::TilePosition &tp, int max_length, struct cmp_buildable_work *p,
									  bool (*foo)( int x, int y, struct cmp_buildable_work *p ) )
{
	const int dirs[4][2] = { { 0, 1 }, { -1, 0 }, { 0, -1}, { 1, 0 } };
	int x        = tp.x();
	int y        = tp.y();
	int x_next_point, y_next_point;
	int x_center = x;
	int y_center = y;
	int idir     = 0;
	int iside    = 0; 

	/* loop of points */
	for ( ;; ) {
		int length = iside / 2 + 1;
		x_next_point = x + length * dirs[ idir % 4 ][0];
		y_next_point = y + length * dirs[ idir % 4 ][1];

		/* loop along edges */
		while ( x != x_next_point || y != y_next_point ) {
			if ( foo( x, y, p ) ) {
				return BWAPI::TilePosition( x, y ); 
			}
			int xdist  = abs( x_center - x );
			int ydist  = abs( y_center - y );
			if ( max_length < xdist && max_length < ydist ) { goto error; }

			x += dirs[ idir % 4 ][0];
			y += dirs[ idir % 4 ][1];
		}
		idir  += 1;
		iside += 1;		
	}

error:
	return BWAPI::TilePositions::Invalid;
}





