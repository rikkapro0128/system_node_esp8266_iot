class EepromMiru
{

public:
  String DATABASE_NODE = "";
  String databaseUrl = "";
  bool canAccess = false;

  EepromMiru(int size, String url_database = "")
  {
    this->size = size;
    this->databaseUrl = url_database;
  }

  void begin()
  {
    this->DATABASE_NODE = this->readDatabaseUrlNode();
    this->checkAccess();
  }

  void resetAll()
  {
    EEPROM.begin(this->size);
    for (unsigned int i = 0; i < this->size; i++)
    {
      EEPROM.write(i, NULL);
    }
    EEPROM.end();
    this->updateDatabaseNode("", "");
  }

  String readRaw()
  {
    EEPROM.begin(this->size);
    String temp;
    for (unsigned int i = 0; i < this->size; i++)
    {
      uint8_t num = EEPROM.read(i);
      temp = String(temp + (char)num);
    }
    EEPROM.end();
    return temp;
  }

  bool saveSSID(String ssid)
  {
    return this->checkWrite(this->addr_ssid, ssid, this->ssid);
  }
  bool savePassword(String password)
  {
    return this->checkWrite(this->addr_password, password, this->password);
  }
  bool saveUserID(String user_id)
  {
    bool state = this->checkWrite(this->addr_userID, user_id, this->userID);
    this->updateDatabaseNode(user_id, this->readNodeID());
    return state;
  }
  bool saveNodeID(String node_id)
  {
    bool state = this->checkWrite(this->addr_NodeID, node_id, this->NodeID);
    this->updateDatabaseNode(this->readUserID(), node_id);
    return state;
  }
  bool saveDatabaseUrl(String url)
  {
    this->maxCharacter = 120;
    bool state = this->checkWrite(this->addr_databaseUrl, url, this->databaseUrl);
    this->maxCharacter = 0;
    return state;
  }

  String readSSID()
  {
    return this->checkRead(this->addr_ssid, this->ssid);
  }
  String readPassword()
  {
    return this->checkRead(this->addr_password, this->password);
  }
  String readNodeID()
  {
    return this->checkRead(this->addr_NodeID, this->NodeID);
  }
  String readUserID()
  {
    return this->checkRead(this->addr_userID, this->userID);
  }
  String readDatabaseUrl()
  {
    this->maxCharacter = 120;
    String temp = this->checkRead(this->addr_databaseUrl, this->databaseUrl);
    this->maxCharacter = 0;
    return temp;
  }
  String readDatabaseUrlNode()
  {
    return String("/user-" + this->readUserID() + "/nodes/node-" + this->readNodeID());
  }

private:
  int size = 1024;
  String ssid = "";
  int addr_ssid = 0;
  String password = "";
  int addr_password = 50;
  String userID = "";
  int addr_userID = 100;
  String NodeID = "";
  int addr_NodeID = 150;
  int addr_databaseUrl = 200;
  uint8_t maxCharacter = 0;

  String checkRead(int addr, String &cacheValue)
  {
    if (cacheValue.length() > 0)
    {
      return cacheValue;
    }
    else
    {
      String temp = this->readKey(addr);
      if (temp != cacheValue)
      {
        cacheValue = temp;
      }
      return temp;
    }
  }

  String readKey(int addr)
  {
    EEPROM.begin(this->size);
    String temp = "";
    for (unsigned int i = addr; i < this->maxCharacter || (addr + 50); i++)
    {
      uint8_t character = EEPROM.read(i);
      if (character <= 126 && character >= 32)
      {
        temp = String(temp + (char)character);
      }
      else
      {
        break;
      }
    }
    EEPROM.end();
    return temp;
  }

  bool writeKey(int addr, String tmp, uint8_t limit = 50)
  {
    int len_str = tmp.length();
    uint8_t checkLimit = this->maxCharacter ? this->maxCharacter : limit;
#ifdef _DEBUG_
    Serial.println(checkLimit);
#endif
    if (len_str > checkLimit)
    {
      return false;
    }
    else
    {
      EEPROM.begin(this->size);
      int start = 0;
      for (uint8_t i = addr; i < addr + checkLimit; i++)
      {
        if (i < (addr + len_str))
        {
          EEPROM.write(i, (uint8_t)tmp[start]);
          start++;
        }
        else
        {
          EEPROM.write(i, NULL);
        }
      }
      EEPROM.commit();
      EEPROM.end();
      return true;
    }
  }

  bool checkWrite(int addr, String tmp, String &cached)
  {
    bool check = this->writeKey(addr, tmp);
    if (check)
    {
      cached = tmp;
    }
    return check;
  }

  void updateDatabaseNode(String uid = "", String nid = "")
  {
    this->DATABASE_NODE = String("/user-" + uid + "/nodes/node-" + nid);
    this->checkAccess();
  }
  void checkAccess()
  {
    String nodeId = this->readNodeID();
    String userId = this->readUserID();
    String dbURL = this->readDatabaseUrl();
    if (nodeId.length() > 0 && userId.length() > 0 && dbURL.length() > 0)
    {
      this->canAccess = true;
    }
    else
    {
      this->canAccess = false;
    }
  }
};

extern EepromMiru
