#pragma once

#include "steam_api.h"

struct async_steamapi_caller
{
	std::vector<CSteamID> lobbies;
	uint32_t player_count;

	CCallResult<async_steamapi_caller, LobbyMatchList_t> call_result_lobby_match_list;
	CCallResult<async_steamapi_caller, NumberOfCurrentPlayers_t> call_player_number;

	void request_lobbies()
	{
		SteamMatchmaking()->AddRequestLobbyListDistanceFilter(k_ELobbyDistanceFilterWorldwide);
		SteamAPICall_t hSteamAPICall = SteamMatchmaking()->RequestLobbyList();
		call_result_lobby_match_list.Set(hSteamAPICall, this, &async_steamapi_caller::on_lobby_match_list);
	}

	void request_player_count()
	{
		SteamAPICall_t hSteamAPICall = SteamUserStats()->GetNumberOfCurrentPlayers();
		call_player_number.Set(hSteamAPICall, this, &async_steamapi_caller::on_get_player_count);
	}

	void on_lobby_match_list(LobbyMatchList_t* pLobbyMatchList, bool bIOFailure)
	{
		lobbies.clear();
		for (int i = 0; i < pLobbyMatchList->m_nLobbiesMatching; i++)
		{
			CSteamID current_lobby = SteamMatchmaking()->GetLobbyByIndex(i);
			lobbies.emplace_back(current_lobby);
		}
	}

	void on_get_player_count(NumberOfCurrentPlayers_t* pNumberOfCurrentPlayers, bool bIOFailure)
	{
		player_count = pNumberOfCurrentPlayers->m_cPlayers;
	}
};