"use strict";

const assert = require("node:assert/strict");
const { EventEmitter, once } = require("node:events");
const http = require("node:http");
const test = require("node:test");
const { WebSocket } = require("ws");
const { CreateRealtimeServer } = require("../realtime");

async function Fixture(T)
{
	const Recognition = new EventEmitter();
	Recognition.Current = () => ({ users: [] });
	Recognition.Reports = [];
	Recognition.Report = (Body, Ip) => Recognition.Reports.push({ Body, Ip });
	Recognition.Leave = () => {};
	const Reports = [];
	const Server = http.createServer((_Req, Res) => Res.writeHead(404).end());
	const Hub = CreateRealtimeServer(Server, {
		Recognition,
		DeveloperService: { ReportPresence: () => ({ statusCode: 401 }), GetPresences: (Address) => ({ response: { server_time: 100, server_address: Address, presences: [] } }) },
		TitleService: { Profile: () => ({ statusCode: 401 }), Report: () => ({ statusCode: 401 }), List: (Address) => ({ response: { server_time: 100, server_address: Address, presences: [] } }) },
		NewsService: { Current: () => ({ response: { version: 7, markdown: "内容" } }) },
		Playtime: (Action, Body) => { Reports.push({ Action, Body }); return { statusCode: 200, response: { action: Action, total_seconds: 10, running: Action !== "stop", last_start_at: 90 } }; },
		NowSec: () => 100
	});
	await new Promise((Resolve) => Server.listen(0, "127.0.0.1", Resolve));
	const Sockets = [];
	T.after(async () => {
		for(const Socket of Sockets) Socket.terminate();
		Hub.Close();
		await new Promise((Resolve) => Server.close(Resolve));
	});
	async function Connect()
	{
		const Socket = new WebSocket(`ws://127.0.0.1:${Server.address().port}/ws`, "qmclient-json");
		Sockets.push(Socket);
		const Messages = [];
		Socket.on("message", (Data) => Messages.push(JSON.parse(Data)));
		await once(Socket, "open");
		return { Socket, Messages };
	}
	return { Connect, Hub, Recognition, Reports };
}

const Hello = (Address = "one:8303") => ({ type: "hello", v: 2, machine_hash: "a".repeat(64), client_id: "qm1234567890", player_name: "玩家", server_address: Address, session_id: "session-1", players: [{ player_id: 1, player_name: "玩家", dummy: false, voice_supported: true }] });

async function WaitFor(Messages, Type)
{
	for(let Attempt = 0; Attempt < 100; ++Attempt)
	{
		const Message = Messages.find((Entry) => Entry.type === Type);
		if(Message) return Message;
		await new Promise((Resolve) => setTimeout(Resolve, 5));
	}
	assert.fail(`没有收到 ${Type}`);
}

test("握手后无需轮询便收到各项初始快照", async (T) => {
	const F = await Fixture(T);
	const C = await F.Connect();
	C.Socket.send(JSON.stringify(Hello()));
	for(const Type of ["users", "developers", "titles", "broadcast", "playtime", "time"])
		await WaitFor(C.Messages, Type);
	assert.equal(F.Recognition.Reports.length, 1);
	assert.equal(F.Reports[0].Action, "start");
});

test("换服后的头衔快照携带新服务器地址", async (T) => {
	const F = await Fixture(T);
	const C = await F.Connect();
	C.Socket.send(JSON.stringify(Hello()));
	await WaitFor(C.Messages, "titles");
	C.Messages.length = 0;
	C.Socket.send(JSON.stringify({ ...Hello("two:8303"), type: "presence" }));
	assert.equal((await WaitFor(C.Messages, "titles")).data.server_address, "two:8303");
});

test("服务端数据变动主动推送且不泄露识别记录的 IP", async (T) => {
	const F = await Fixture(T);
	const C = await F.Connect();
	C.Socket.send(JSON.stringify(Hello()));
	await WaitFor(C.Messages, "users");
	C.Messages.length = 0;
	F.Recognition.emit("users", { users: [{ server_address: "one:8303", player_name: "另一个玩家", last_ip: "192.0.2.1", qid: "qid" }] });
	const Message = await WaitFor(C.Messages, "users");
	assert.equal(Message.data.users[0].player_name, "另一个玩家");
	assert.equal(Message.data.users[0].last_ip, undefined);
});

test("未握手的连接不能发布 presence", async (T) => {
	const F = await Fixture(T);
	const C = await F.Connect();
	C.Socket.send(JSON.stringify({ ...Hello(), type: "presence" }));
	await WaitFor(C.Messages, "error");
	assert.equal(F.Recognition.Reports.length, 0);
	assert.equal(F.Reports.length, 0);
});

test("重连重新下发快照，已连连接不能替换游玩时长身份", async (T) => {
	const F = await Fixture(T);
	const C = await F.Connect();
	C.Socket.send(JSON.stringify(Hello()));
	await WaitFor(C.Messages, "playtime");
	C.Messages.length = 0;
	C.Socket.send(JSON.stringify({ ...Hello(), type: "presence", client_id: "qmSomeoneElse" }));
	C.Socket.send(JSON.stringify({ type: "stop", stop_at: 100 }));
	await WaitFor(C.Messages, "playtime");
	assert.equal(F.Reports.at(-1).Action, "stop");
	assert.equal(F.Reports.at(-1).Body.client_id, Hello().client_id);
	const Other = await F.Connect();
	Other.Socket.send(JSON.stringify(Hello()));
	await WaitFor(Other.Messages, "broadcast");
	await WaitFor(Other.Messages, "users");
});

test("表情事件只广播给同一游戏服务器，且不回放历史", async (T) => {
	const F = await Fixture(T);
	const One = await F.Connect();
	const Two = await F.Connect();
	const OtherRoom = await F.Connect();
	One.Socket.send(JSON.stringify(Hello("one:8303")));
	Two.Socket.send(JSON.stringify({ ...Hello("one:8303"), client_id: "qm2222222222", session_id: "session-2", player_name: "另一个玩家", players: [{ player_id: 2, player_name: "另一个玩家", dummy: false }] }));
	OtherRoom.Socket.send(JSON.stringify({ ...Hello("two:8303"), client_id: "qm3333333333", session_id: "session-3", player_name: "其他房间", players: [{ player_id: 3, player_name: "其他房间", dummy: false }] }));
	await WaitFor(One.Messages, "users");
	await WaitFor(Two.Messages, "users");
	await WaitFor(OtherRoom.Messages, "users");
	One.Messages.length = 0;
	Two.Messages.length = 0;
	OtherRoom.Messages.length = 0;

	One.Socket.send(JSON.stringify({ type: "emoticon", emoticon: 4, player_id: 1, launch_mode: true, super_launch: true }));
	const Event = await WaitFor(Two.Messages, "emoticon");
	assert.deepEqual(Event.data, {
		client_id: "qm1234567890",
		player_id: 1,
		player_name: "玩家",
		server_address: "one:8303",
		emoticon: 4,
		launch_mode: true,
		super_launch: true,
		sequence: 1
	});
	assert.equal(One.Messages.some((Message) => Message.type === "emoticon"), true);
	await new Promise((Resolve) => setTimeout(25));
	assert.equal(OtherRoom.Messages.some((Message) => Message.type === "emoticon"), false);
});

test("表情事件必须来自当前握手声明的玩家", async (T) => {
	const F = await Fixture(T);
	const C = await F.Connect();
	C.Socket.send(JSON.stringify(Hello()));
	await WaitFor(C.Messages, "users");
	C.Messages.length = 0;
	C.Socket.send(JSON.stringify({ type: "emoticon", emoticon: 4, player_id: 7, launch_mode: true, super_launch: false }));
	const Error = await WaitFor(C.Messages, "error");
	assert.equal(Error.data.error, "invalid_emoticon_player");
});
