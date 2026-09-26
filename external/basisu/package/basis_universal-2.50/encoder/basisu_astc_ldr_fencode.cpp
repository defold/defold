// basisu_astc_ldr_fencode.cpp
#include "basisu_astc_ldr_fencode.h"
#include "../transcoder/basisu_astc_helpers.h"
#include "basisu_astc_ldr_encode.h"
#include "basisu_enc.h"
#include <algorithm>

namespace basisu
{
namespace astc_ldrf
{

// cem: 4 bits, ofs 0
// w: 4 bits, ofs 4
// h: 4 bits, ofs 8
// e: 5 bits, ofs 12
// w: 4 bits, ofs 17
// dp: 1 bit, ofs 21

// cems 6,8,10,12 only, gw>=gh (opposite case of gw<gh handled in code), dp ccs index handled in code
const uint32_t TOTAL_SINGLE_SUBSET_CONFIGS_RGBA = 1589;
static const uint32_t g_single_subset_configs_rgba[TOTAL_SINGLE_SUBSET_CONFIGS_RGBA] =
{
	0x14556, 0x14466, 0x14566, 0x14666, 0x14476, 0x14576, 0x14676, 0x14776, 0x14386, 0x14486, 0x14586, 0x14686, 0x14786, 0x14886, 0x14396, 0x14496, 0x14596, 0x14696, 0x14796, 0x143a6, 0x144a6, 0x145a6, 0x146a6, 0x143b6, 0x144b6, 0x145b6, 0x142c6, 0x143c6, 0x144c6, 0x145c6, 0x14558, 0x14468,
	0x14568, 0x14668, 0x14478, 0x14578, 0x14678, 0x14778, 0x14388, 0x14488, 0x14588, 0x14688, 0x14788, 0x13888, 0x14398, 0x14498, 0x14598, 0x14698, 0x14798, 0x143a8, 0x144a8, 0x145a8, 0x146a8, 0x143b8, 0x144b8, 0x145b8, 0x142c8, 0x143c8, 0x144c8, 0x145c8, 0x1455a, 0x1446a, 0x1456a, 0x1466a,
	0x1447a, 0x1457a, 0x1467a, 0x1477a, 0x1438a, 0x1448a, 0x1458a, 0x1468a, 0x1478a, 0x1388a, 0x1439a, 0x1449a, 0x1459a, 0x1469a, 0x1479a, 0x143aa, 0x144aa, 0x145aa, 0x146aa, 0x143ba, 0x144ba, 0x145ba, 0x142ca, 0x143ca, 0x144ca, 0x145ca, 0x1455c, 0x1446c, 0x1456c, 0x1466c, 0x1447c, 0x1457c,
	0x1467c, 0x1377c, 0x1438c, 0x1448c, 0x1458c, 0x1368c, 0x1078c, 0xd88c, 0x1439c, 0x1449c, 0x1459c, 0x1169c, 0xe79c, 0x143ac, 0x144ac, 0x135ac, 0xf6ac, 0x143bc, 0x144bc, 0x115bc, 0x142cc, 0x143cc, 0x134cc, 0xf5cc, 0x34446, 0x34356, 0x34456, 0x34556, 0x34366, 0x34466, 0x34566, 0x34666,
	0x34376, 0x34476, 0x34576, 0x34676, 0x34776, 0x34286, 0x34386, 0x34486, 0x34586, 0x34686, 0x2b786, 0x34296, 0x34396, 0x34496, 0x34596, 0x2e696, 0x342a6, 0x343a6, 0x344a6, 0x335a6, 0x276a6, 0x342b6, 0x343b6, 0x344b6, 0x2d5b6, 0x342c6, 0x343c6, 0x344c6, 0x275c6, 0x34448, 0x34358, 0x34458,
	0x34558, 0x34368, 0x34468, 0x34568, 0x34668, 0x34378, 0x34478, 0x34578, 0x31678, 0x2c778, 0x34288, 0x34388, 0x34488, 0x33588, 0x2d688, 0x26788, 0x34298, 0x34398, 0x34498, 0x2f598, 0x28698, 0x342a8, 0x343a8, 0x334a8, 0x2b5a8, 0x342b8, 0x343b8, 0x304b8, 0x275b8, 0x342c8, 0x343c8, 0x2d4c8,
	0x3444a, 0x3435a, 0x3445a, 0x3455a, 0x3436a, 0x3446a, 0x3456a, 0x3466a, 0x3437a, 0x3447a, 0x3457a, 0x3167a, 0x2c77a, 0x3428a, 0x3438a, 0x3448a, 0x3358a, 0x2d68a, 0x2678a, 0x3429a, 0x3439a, 0x3449a, 0x2f59a, 0x2869a, 0x342aa, 0x343aa, 0x334aa, 0x2b5aa, 0x342ba, 0x343ba, 0x304ba, 0x275ba,
	0x342ca, 0x343ca, 0x2d4ca, 0x3444c, 0x3435c, 0x3445c, 0x3455c, 0x3436c, 0x3446c, 0x3356c, 0x3066c, 0x3437c, 0x3447c, 0x3057c, 0x2c67c, 0x2877c, 0x3428c, 0x3438c, 0x3248c, 0x2d58c, 0x2868c, 0x2478c, 0x3429c, 0x3439c, 0x3049c, 0x2a59c, 0x2569c, 0x342ac, 0x333ac, 0x2d4ac, 0x275ac, 0x342bc,
	0x313bc, 0x2b4bc, 0x245bc, 0x342cc, 0x303cc, 0x284cc, 0x54346, 0x54446, 0x54356, 0x54456, 0x54556, 0x54266, 0x54366, 0x54466, 0x54566, 0x54666, 0x54276, 0x54376, 0x54476, 0x54576, 0x50676, 0x54286, 0x54386, 0x54486, 0x53586, 0x47686, 0x54296, 0x54396, 0x54496, 0x4b596, 0x542a6, 0x543a6,
	0x534a6, 0x542b6, 0x543b6, 0x4d4b6, 0x542c6, 0x543c6, 0x474c6, 0x54348, 0x54448, 0x54358, 0x54458, 0x54558, 0x54268, 0x54368, 0x54468, 0x54568, 0x4f668, 0x54278, 0x54378, 0x54478, 0x50578, 0x49678, 0x54288, 0x54388, 0x53488, 0x4b588, 0x54298, 0x54398, 0x4f498, 0x46598, 0x542a8, 0x543a8,
	0x4b4a8, 0x542b8, 0x523b8, 0x474b8, 0x542c8, 0x4f3c8, 0x5434a, 0x5444a, 0x5435a, 0x5445a, 0x5455a, 0x5426a, 0x5436a, 0x5446a, 0x5456a, 0x4f66a, 0x5427a, 0x5437a, 0x5447a, 0x5057a, 0x4967a, 0x5428a, 0x5438a, 0x5348a, 0x4b58a, 0x5429a, 0x5439a, 0x4f49a, 0x4659a, 0x542aa, 0x543aa, 0x4b4aa,
	0x542ba, 0x523ba, 0x474ba, 0x542ca, 0x4f3ca, 0x5434c, 0x5444c, 0x5435c, 0x5445c, 0x5355c, 0x5426c, 0x5436c, 0x5346c, 0x4f56c, 0x4a66c, 0x5427c, 0x5437c, 0x5047c, 0x4b57c, 0x4667c, 0x5428c, 0x5338c, 0x4d48c, 0x4758c, 0x5429c, 0x5139c, 0x4a49c, 0x4459c, 0x542ac, 0x4f3ac, 0x474ac, 0x542bc,
	0x4d3bc, 0x444bc, 0x532cc, 0x4a3cc, 0x74346, 0x74446, 0x74256, 0x74356, 0x74456, 0x74556, 0x74266, 0x74366, 0x74466, 0x74566, 0x70666, 0x74276, 0x74376, 0x74476, 0x71576, 0x74286, 0x74386, 0x74486, 0x68586, 0x74296, 0x74396, 0x70496, 0x742a6, 0x743a6, 0x684a6, 0x742b6, 0x743b6, 0x742c6,
	0x703c6, 0x74348, 0x74448, 0x74258, 0x74358, 0x74458, 0x74558, 0x74268, 0x74368, 0x74468, 0x70568, 0x69668, 0x74278, 0x74378, 0x72478, 0x6a578, 0x74288, 0x74388, 0x6e488, 0x64588, 0x74298, 0x74398, 0x69498, 0x742a8, 0x703a8, 0x644a8, 0x742b8, 0x6d3b8, 0x742c8, 0x693c8, 0x7434a, 0x7444a,
	0x7425a, 0x7435a, 0x7445a, 0x7455a, 0x7426a, 0x7436a, 0x7446a, 0x7056a, 0x6966a, 0x7427a, 0x7437a, 0x7247a, 0x6a57a, 0x7428a, 0x7438a, 0x6e48a, 0x6458a, 0x7429a, 0x7439a, 0x6949a, 0x742aa, 0x703aa, 0x644aa, 0x742ba, 0x6d3ba, 0x742ca, 0x693ca, 0x7434c, 0x7444c, 0x7425c, 0x7435c, 0x7445c,
	0x6f55c, 0x7426c, 0x7436c, 0x7046c, 0x6b56c, 0x6666c, 0x7427c, 0x7337c, 0x6d47c, 0x6757c, 0x7428c, 0x7038c, 0x6948c, 0x7429c, 0x6e39c, 0x6649c, 0x742ac, 0x6b3ac, 0x722bc, 0x683bc, 0x702cc, 0x663cc, 0x94336, 0x94346, 0x94446, 0x94256, 0x94356, 0x94456, 0x94556, 0x94266, 0x94366, 0x94466,
	0x94566, 0x88666, 0x94276, 0x94376, 0x94476, 0x8b576, 0x94286, 0x94386, 0x90486, 0x94296, 0x94396, 0x88496, 0x942a6, 0x943a6, 0x942b6, 0x8e3b6, 0x942c6, 0x883c6, 0x94338, 0x94348, 0x94448, 0x94258, 0x94358, 0x94458, 0x93558, 0x94268, 0x94368, 0x94468, 0x8c568, 0x84668, 0x94278, 0x94378,
	0x8f478, 0x86578, 0x94288, 0x94388, 0x89488, 0x94298, 0x90398, 0x84498, 0x942a8, 0x8c3a8, 0x942b8, 0x883b8, 0x942c8, 0x843c8, 0x9433a, 0x9434a, 0x9444a, 0x9425a, 0x9435a, 0x9445a, 0x9355a, 0x9426a, 0x9436a, 0x9446a, 0x8c56a, 0x8466a, 0x9427a, 0x9437a, 0x8f47a, 0x8657a, 0x9428a, 0x9438a,
	0x8948a, 0x9429a, 0x9039a, 0x8449a, 0x942aa, 0x8c3aa, 0x942ba, 0x883ba, 0x942ca, 0x843ca, 0x9433c, 0x9434c, 0x9444c, 0x9425c, 0x9435c, 0x9245c, 0x8d55c, 0x9426c, 0x9436c, 0x8e46c, 0x8856c, 0x9427c, 0x9137c, 0x8a47c, 0x9428c, 0x8e38c, 0x8648c, 0x9429c, 0x8b39c, 0x922ac, 0x883ac, 0x902bc,
	0x853bc, 0x8e2cc, 0xb4336, 0xb4246, 0xb4346, 0xb4446, 0xb4256, 0xb4356, 0xb4456, 0xb4556, 0xb4266, 0xb4366, 0xb4466, 0xab566, 0xb4276, 0xb4376, 0xb0476, 0xb4286, 0xb4386, 0xa7486, 0xb4296, 0xb2396, 0xb42a6, 0xab3a6, 0xb42b6, 0xb42c6, 0xb4338, 0xb4248, 0xb4348, 0xb4448, 0xb4258, 0xb4358,
	0xb4458, 0xae558, 0xb4268, 0xb4368, 0xaf468, 0xa6568, 0xb4278, 0xb4378, 0xa9478, 0xb4288, 0xaf388, 0xb4298, 0xab398, 0xb42a8, 0xa63a8, 0xb22b8, 0xaf2c8, 0xb433a, 0xb424a, 0xb434a, 0xb444a, 0xb425a, 0xb435a, 0xb445a, 0xae55a, 0xb426a, 0xb436a, 0xaf46a, 0xa656a, 0xb427a, 0xb437a, 0xa947a,
	0xb428a, 0xaf38a, 0xb429a, 0xab39a, 0xb42aa, 0xa63aa, 0xb22ba, 0xaf2ca, 0xb433c, 0xb424c, 0xb434c, 0xb344c, 0xb425c, 0xb435c, 0xaf45c, 0xa955c, 0xb426c, 0xb136c, 0xaa46c, 0xa456c, 0xb427c, 0xae37c, 0xa647c, 0xb328c, 0xaa38c, 0xb129c, 0xa739c, 0xaf2ac, 0xa43ac, 0xad2bc, 0xaa2cc, 0xd4336,
	0xd4246, 0xd4346, 0xd4446, 0xd4256, 0xd4356, 0xd4456, 0xd0556, 0xd4266, 0xd4366, 0xd3466, 0xd4276, 0xd4376, 0xc8476, 0xd4286, 0xd3386, 0xd4296, 0xcb396, 0xd42a6, 0xd42b6, 0xd32c6, 0xd4338, 0xd4248, 0xd4348, 0xd4448, 0xd4258, 0xd4358, 0xd2458, 0xc9558, 0xd4268, 0xd4368, 0xcb468, 0xd4278,
	0xd0378, 0xc4478, 0xd4288, 0xcb388, 0xd4298, 0xc6398, 0xd22a8, 0xce2b8, 0xcb2c8, 0xd433a, 0xd424a, 0xd434a, 0xd444a, 0xd425a, 0xd435a, 0xd245a, 0xc955a, 0xd426a, 0xd436a, 0xcb46a, 0xd427a, 0xd037a, 0xc447a, 0xd428a, 0xcb38a, 0xd429a, 0xc639a, 0xd22aa, 0xce2ba, 0xcb2ca, 0xd433c, 0xd424c,
	0xd434c, 0xd144c, 0xd425c, 0xd335c, 0xcc45c, 0xc655c, 0xd426c, 0xcf36c, 0xc746c, 0xd427c, 0xcb37c, 0xd128c, 0xc738c, 0xcf29c, 0xc439c, 0xcc2ac, 0xca2bc, 0xc72cc, 0xf4336, 0xf4246, 0xf4346, 0xf4446, 0xf4256, 0xf4356, 0xf4456, 0xeb556, 0xf4266, 0xf4366, 0xee466, 0xf4276, 0xf4376, 0xf4286,
	0xee386, 0xf4296, 0xf42a6, 0xf32b6, 0xee2c6, 0xf4338, 0xf4248, 0xf4348, 0xf4448, 0xf4258, 0xf4358, 0xef458, 0xe6558, 0xf4268, 0xf3368, 0xe8468, 0xf4278, 0xed378, 0xf4288, 0xe8388, 0xf3298, 0xef2a8, 0xeb2b8, 0xe82c8, 0xf433a, 0xf424a, 0xf434a, 0xf444a, 0xf425a, 0xf435a, 0xef45a, 0xe655a,
	0xf426a, 0xf336a, 0xe846a, 0xf427a, 0xed37a, 0xf428a, 0xe838a, 0xf329a, 0xef2aa, 0xeb2ba, 0xe82ca, 0xf433c, 0xf424c, 0xf434c, 0xf044c, 0xf425c, 0xf135c, 0xea45c, 0xe455c, 0xf426c, 0xed36c, 0xe546c, 0xf227c, 0xe937c, 0xf028c, 0xe538c, 0xed29c, 0xea2ac, 0xe72bc, 0xe52cc, 0x114236, 0x114336,
	0x114246, 0x114346, 0x114446, 0x114256, 0x114356, 0x113456, 0x114266, 0x114366, 0x107466, 0x114276, 0x110376, 0x114286, 0x107386, 0x114296, 0x1132a6, 0x10d2b6, 0x1072c6, 0x114238, 0x114338, 0x114248, 0x114348, 0x113448, 0x114258, 0x114358, 0x10b458, 0x114268, 0x10f368, 0x114278, 0x109378, 0x113288, 0x10f298, 0x10b2a8,
	0x1072b8, 0x11423a, 0x11433a, 0x11424a, 0x11434a, 0x11344a, 0x11425a, 0x11435a, 0x10b45a, 0x11426a, 0x10f36a, 0x11427a, 0x10937a, 0x11328a, 0x10f29a, 0x10b2aa, 0x1072ba, 0x11423c, 0x11433c, 0x11424c, 0x11334c, 0x10d44c, 0x11425c, 0x10f35c, 0x10745c, 0x11326c, 0x10a36c, 0x11027c, 0x10637c, 0x10d28c, 0x10a29c, 0x1072ac,
	0x1042bc, 0x134236, 0x134336, 0x134246, 0x134346, 0x134446, 0x134256, 0x134356, 0x12e456, 0x134266, 0x134366, 0x134276, 0x12b376, 0x134286, 0x134296, 0x12e2a6, 0x1272b6, 0x134238, 0x134338, 0x134248, 0x134348, 0x130448, 0x134258, 0x133358, 0x128458, 0x134268, 0x12c368, 0x134278, 0x126378, 0x130288, 0x12c298, 0x1282a8,
	0x13423a, 0x13433a, 0x13424a, 0x13434a, 0x13044a, 0x13425a, 0x13335a, 0x12845a, 0x13426a, 0x12c36a, 0x13427a, 0x12637a, 0x13028a, 0x12c29a, 0x1282aa, 0x13423c, 0x13433c, 0x13424c, 0x13234c, 0x12b44c, 0x13425c, 0x12d35c, 0x12545c, 0x13226c, 0x12836c, 0x12e27c, 0x12b28c, 0x12829c, 0x1252ac, 0x154236, 0x154336, 0x154246,
	0x154346, 0x154446, 0x154256, 0x154356, 0x14a456, 0x154266, 0x151366, 0x154276, 0x154286, 0x151296, 0x14a2a6, 0x154238, 0x154338, 0x154248, 0x154348, 0x14e448, 0x154258, 0x151358, 0x145458, 0x154268, 0x14a368, 0x153278, 0x14e288, 0x14a298, 0x1452a8, 0x15423a, 0x15433a, 0x15424a, 0x15434a, 0x14e44a, 0x15425a, 0x15135a,
	0x14545a, 0x15426a, 0x14a36a, 0x15327a, 0x14e28a, 0x14a29a, 0x1452aa, 0x15423c, 0x15433c, 0x15424c, 0x15034c, 0x14a44c, 0x15425c, 0x14b35c, 0x15026c, 0x14636c, 0x14d27c, 0x14a28c, 0x14629c, 0x174236, 0x174336, 0x174246, 0x174346, 0x173446, 0x174256, 0x174356, 0x174266, 0x16b366, 0x174276, 0x173286, 0x16b296, 0x174238,
	0x174338, 0x174248, 0x174348, 0x16b448, 0x174258, 0x16e358, 0x174268, 0x166368, 0x170278, 0x16b288, 0x166298, 0x17423a, 0x17433a, 0x17424a, 0x17434a, 0x16b44a, 0x17425a, 0x16e35a, 0x17426a, 0x16636a, 0x17027a, 0x16b28a, 0x16629a, 0x17423c, 0x17433c, 0x17424c, 0x16f34c, 0x16744c, 0x17325c, 0x16935c, 0x16f26c, 0x16436c,
	0x16b27c, 0x16728c, 0x16429c, 0x214346, 0x214446, 0x214356, 0x214456, 0x214556, 0x214266, 0x214366, 0x214466, 0x214566, 0x214276, 0x214376, 0x214476, 0x214286, 0x214386, 0x214486, 0x214296, 0x214396, 0x2142a6, 0x2143a6, 0x2142b6, 0x2142c6, 0x214348, 0x214448, 0x214358, 0x214458, 0x214558, 0x214268, 0x214368, 0x214468,
	0x214568, 0x214278, 0x214378, 0x214478, 0x214288, 0x214388, 0x212488, 0x214298, 0x214398, 0x2142a8, 0x2143a8, 0x2142b8, 0x2142c8, 0x21434a, 0x21444a, 0x21435a, 0x21445a, 0x21455a, 0x21426a, 0x21436a, 0x21446a, 0x21456a, 0x21427a, 0x21437a, 0x21447a, 0x21428a, 0x21438a, 0x21248a, 0x21429a, 0x21439a, 0x2142aa, 0x2143aa,
	0x2142ba, 0x2142ca, 0x21434c, 0x21444c, 0x21435c, 0x21445c, 0x21255c, 0x21426c, 0x21436c, 0x21346c, 0x20e56c, 0x21427c, 0x21437c, 0x21047c, 0x21428c, 0x21338c, 0x20d48c, 0x21429c, 0x21039c, 0x2142ac, 0x20e3ac, 0x2142bc, 0x2132cc, 0x234336, 0x234246, 0x234346, 0x234446, 0x234256, 0x234356, 0x234456, 0x231556, 0x234266,
	0x234366, 0x234466, 0x225566, 0x234276, 0x234376, 0x22a476, 0x234286, 0x234386, 0x234296, 0x22c396, 0x2342a6, 0x2253a6, 0x2342b6, 0x2342c6, 0x234338, 0x234248, 0x234348, 0x234448, 0x234258, 0x234358, 0x232458, 0x22a558, 0x234268, 0x234368, 0x22c468, 0x234278, 0x230378, 0x225478, 0x234288, 0x22c388, 0x234298, 0x227398,
	0x2322a8, 0x22f2b8, 0x22c2c8, 0x23433a, 0x23424a, 0x23434a, 0x23444a, 0x23425a, 0x23435a, 0x23245a, 0x22a55a, 0x23426a, 0x23436a, 0x22c46a, 0x23427a, 0x23037a, 0x22547a, 0x23428a, 0x22c38a, 0x23429a, 0x22739a, 0x2322aa, 0x22f2ba, 0x22c2ca, 0x23433c, 0x23424c, 0x23434c, 0x23144c, 0x23425c, 0x23335c, 0x22d45c, 0x22755c,
	0x23426c, 0x22f36c, 0x22846c, 0x23427c, 0x22b37c, 0x23128c, 0x22838c, 0x22f29c, 0x22439c, 0x22d2ac, 0x22a2bc, 0x2282cc, 0x254236, 0x254336, 0x254246, 0x254346, 0x254446, 0x254256, 0x254356, 0x251456, 0x254266, 0x254366, 0x245466, 0x254276, 0x24e376, 0x254286, 0x245386, 0x254296, 0x2512a6, 0x24b2b6, 0x2452c6, 0x254238,
	0x254338, 0x254248, 0x254348, 0x252448, 0x254258, 0x254358, 0x24a458, 0x254268, 0x24e368, 0x254278, 0x248378, 0x252288, 0x24e298, 0x24a2a8, 0x2462b8, 0x25423a, 0x25433a, 0x25424a, 0x25434a, 0x25244a, 0x25425a, 0x25435a, 0x24a45a, 0x25426a, 0x24e36a, 0x25427a, 0x24837a, 0x25228a, 0x24e29a, 0x24a2aa, 0x2462ba, 0x25423c,
	0x25433c, 0x25424c, 0x25334c, 0x24d44c, 0x25425c, 0x24e35c, 0x24745c, 0x25326c, 0x24a36c, 0x25027c, 0x24537c, 0x24d28c, 0x24a29c, 0x2472ac, 0x2442bc, 0x274236, 0x274336, 0x274246, 0x274346, 0x274446, 0x274256, 0x274356, 0x267456, 0x274266, 0x26e366, 0x274276, 0x274286, 0x26e296, 0x2672a6, 0x274238, 0x274338, 0x274248,
	0x274348, 0x26d448, 0x274258, 0x26f358, 0x274268, 0x268368, 0x271278, 0x26d288, 0x268298, 0x27423a, 0x27433a, 0x27424a, 0x27434a, 0x26d44a, 0x27425a, 0x26f35a, 0x27426a, 0x26836a, 0x27127a, 0x26d28a, 0x26829a, 0x27423c, 0x27433c, 0x27424c, 0x27034c, 0x26844c, 0x27325c, 0x26a35c, 0x27026c, 0x26536c, 0x26c27c, 0x26828c,
	0x26529c, 0x294236, 0x294336, 0x294246, 0x294346, 0x28e446, 0x294256, 0x293356, 0x294266, 0x287366, 0x294276, 0x28e286, 0x287296, 0x294238, 0x294338, 0x294248, 0x293348, 0x288448, 0x294258, 0x28b358, 0x293268, 0x28e278, 0x288288, 0x29423a, 0x29433a, 0x29424a, 0x29334a, 0x28844a, 0x29425a, 0x28b35a, 0x29326a, 0x28e27a,
	0x28828a, 0x29423c, 0x29333c, 0x29424c, 0x28d34c, 0x28544c, 0x29125c, 0x28735c, 0x28d26c, 0x28927c, 0x28528c, 0x2b4226, 0x2b4236, 0x2b4336, 0x2b4246, 0x2b4346, 0x2a5446, 0x2b4256, 0x2aa356, 0x2b4266, 0x2ae276, 0x2a5286, 0x2b4228, 0x2b4238, 0x2b4338, 0x2b4248, 0x2ae348, 0x2b4258, 0x2a5358, 0x2ae268, 0x2a8278, 0x2b422a,
	0x2b423a, 0x2b433a, 0x2b424a, 0x2ae34a, 0x2b425a, 0x2a535a, 0x2ae26a, 0x2a827a, 0x2b422c, 0x2b423c, 0x2b033c, 0x2b324c, 0x2aa34c, 0x2ae25c, 0x2aa26c, 0x2a527c, 0x2d4226, 0x2d4236, 0x2d4336, 0x2d4246, 0x2d1346, 0x2d4256, 0x2d1266, 0x2c7276, 0x2d4228, 0x2d4238, 0x2d4338, 0x2d4248, 0x2ca348, 0x2d1258, 0x2ca268, 0x2d422a,
	0x2d423a, 0x2d433a, 0x2d424a, 0x2ca34a, 0x2d125a, 0x2ca26a, 0x2d422c, 0x2d423c, 0x2ce33c, 0x2d024c, 0x2c734c, 0x2cb25c, 0x2c726c, 0x2f4226, 0x2f4236, 0x2f4336, 0x2f4246, 0x2ec346, 0x2f4256, 0x2ec266, 0x2f4228, 0x2f4238, 0x2f2338, 0x2f4248, 0x2e7348, 0x2ee258, 0x2e7268, 0x2f422a, 0x2f423a, 0x2f233a, 0x2f424a, 0x2e734a,
	0x2ee25a, 0x2e726a, 0x2f422c, 0x2f423c, 0x2ec33c, 0x2ef24c, 0x2e434c, 0x2ea25c, 0x2e426c, 0x314226, 0x314236, 0x314336, 0x314246, 0x305346, 0x311256, 0x305266, 0x314228, 0x314238, 0x30e338, 0x312248, 0x30a258, 0x31422a, 0x31423a, 0x30e33a, 0x31224a, 0x30a25a, 0x31422c, 0x31323c, 0x30a33c, 0x30d24c, 0x30725c, 0x334226,
	0x334236, 0x333336, 0x334246, 0x32c256, 0x334228, 0x334238, 0x32b338, 0x32f248, 0x327258, 0x33422a, 0x33423a, 0x32b33a, 0x32f24a, 0x32725a, 0x33422c, 0x33123c, 0x32733c, 0x32a24c, 0x32425c, 0x354226, 0x354236, 0x34f336, 0x354246, 0x348256, 0x354228, 0x354238, 0x349338, 0x34d248, 0x344258, 0x35422a, 0x35423a, 0x34933a,
	0x34d24a, 0x34425a, 0x35422c, 0x35023c, 0x34533c, 0x34924c, 0x374226, 0x374236, 0x36a336, 0x371246, 0x374228, 0x374238, 0x365338, 0x36a248, 0x37422a, 0x37423a, 0x36533a, 0x36a24a, 0x37422c, 0x36e23c, 0x36724c
};

// cems 0,4 only, gw>=gh (opposite case of gw<gh handled in code)
const uint32_t TOTAL_SINGLE_SUBSET_CONFIGS_LA = 700;
static const uint32_t g_single_subset_configs_la[TOTAL_SINGLE_SUBSET_CONFIGS_LA] =
{
	0x14550, 0x14460, 0x14560, 0x14660, 0x14470, 0x14570, 0x14670, 0x14770, 0x14380, 0x14480, 0x14580, 0x14680, 0x14780, 0x14880, 0x14390, 0x14490, 0x14590, 0x14690, 0x14790, 0x143a0, 0x144a0, 0x145a0, 0x146a0, 0x143b0, 0x144b0, 0x145b0, 0x142c0, 0x143c0, 0x144c0, 0x145c0, 0x14554, 0x14464,
	0x14564, 0x14664, 0x14474, 0x14574, 0x14674, 0x14774, 0x14384, 0x14484, 0x14584, 0x14684, 0x14784, 0x14884, 0x14394, 0x14494, 0x14594, 0x14694, 0x14794, 0x143a4, 0x144a4, 0x145a4, 0x146a4, 0x143b4, 0x144b4, 0x145b4, 0x142c4, 0x143c4, 0x144c4, 0x145c4, 0x34440, 0x34350, 0x34450, 0x34550,
	0x34360, 0x34460, 0x34560, 0x34660, 0x34370, 0x34470, 0x34570, 0x34670, 0x34770, 0x34280, 0x34380, 0x34480, 0x34580, 0x34680, 0x34780, 0x34290, 0x34390, 0x34490, 0x34590, 0x34690, 0x342a0, 0x343a0, 0x344a0, 0x345a0, 0x326a0, 0x342b0, 0x343b0, 0x344b0, 0x345b0, 0x342c0, 0x343c0, 0x344c0,
	0x325c0, 0x34444, 0x34354, 0x34454, 0x34554, 0x34364, 0x34464, 0x34564, 0x34664, 0x34374, 0x34474, 0x34574, 0x34674, 0x34774, 0x34284, 0x34384, 0x34484, 0x34584, 0x34684, 0x2b784, 0x34294, 0x34394, 0x34494, 0x34594, 0x2e694, 0x342a4, 0x343a4, 0x344a4, 0x335a4, 0x276a4, 0x342b4, 0x343b4,
	0x344b4, 0x2d5b4, 0x342c4, 0x343c4, 0x344c4, 0x275c4, 0x54340, 0x54440, 0x54350, 0x54450, 0x54550, 0x54260, 0x54360, 0x54460, 0x54560, 0x54660, 0x54270, 0x54370, 0x54470, 0x54570, 0x54670, 0x54280, 0x54380, 0x54480, 0x54580, 0x52680, 0x54290, 0x54390, 0x54490, 0x54590, 0x542a0, 0x543a0,
	0x544a0, 0x542b0, 0x543b0, 0x544b0, 0x542c0, 0x543c0, 0x524c0, 0x54344, 0x54444, 0x54354, 0x54454, 0x54554, 0x54264, 0x54364, 0x54464, 0x54564, 0x54664, 0x54274, 0x54374, 0x54474, 0x54574, 0x50674, 0x54284, 0x54384, 0x54484, 0x53584, 0x47684, 0x54294, 0x54394, 0x54494, 0x4b594, 0x542a4,
	0x543a4, 0x534a4, 0x542b4, 0x543b4, 0x4d4b4, 0x542c4, 0x543c4, 0x474c4, 0x74340, 0x74440, 0x74250, 0x74350, 0x74450, 0x74550, 0x74260, 0x74360, 0x74460, 0x74560, 0x74660, 0x74270, 0x74370, 0x74470, 0x74570, 0x74280, 0x74380, 0x74480, 0x74580, 0x74290, 0x74390, 0x74490, 0x742a0, 0x743a0,
	0x744a0, 0x742b0, 0x743b0, 0x742c0, 0x743c0, 0x74344, 0x74444, 0x74254, 0x74354, 0x74454, 0x74554, 0x74264, 0x74364, 0x74464, 0x74564, 0x70664, 0x74274, 0x74374, 0x74474, 0x71574, 0x74284, 0x74384, 0x74484, 0x68584, 0x74294, 0x74394, 0x70494, 0x742a4, 0x743a4, 0x684a4, 0x742b4, 0x743b4,
	0x742c4, 0x703c4, 0x94330, 0x94340, 0x94440, 0x94250, 0x94350, 0x94450, 0x94550, 0x94260, 0x94360, 0x94460, 0x94560, 0x94660, 0x94270, 0x94370, 0x94470, 0x94570, 0x94280, 0x94380, 0x94480, 0x94290, 0x94390, 0x94490, 0x942a0, 0x943a0, 0x942b0, 0x943b0, 0x942c0, 0x943c0, 0x94334, 0x94344,
	0x94444, 0x94254, 0x94354, 0x94454, 0x94554, 0x94264, 0x94364, 0x94464, 0x94564, 0x88664, 0x94274, 0x94374, 0x94474, 0x8b574, 0x94284, 0x94384, 0x90484, 0x94294, 0x94394, 0x88494, 0x942a4, 0x943a4, 0x942b4, 0x8e3b4, 0x942c4, 0x883c4, 0xb4330, 0xb4240, 0xb4340, 0xb4440, 0xb4250, 0xb4350,
	0xb4450, 0xb4550, 0xb4260, 0xb4360, 0xb4460, 0xb4560, 0xb4270, 0xb4370, 0xb4470, 0xb4280, 0xb4380, 0xb2480, 0xb4290, 0xb4390, 0xb42a0, 0xb43a0, 0xb42b0, 0xb42c0, 0xb4334, 0xb4244, 0xb4344, 0xb4444, 0xb4254, 0xb4354, 0xb4454, 0xb4554, 0xb4264, 0xb4364, 0xb4464, 0xab564, 0xb4274, 0xb4374,
	0xb0474, 0xb4284, 0xb4384, 0xa7484, 0xb4294, 0xb2394, 0xb42a4, 0xab3a4, 0xb42b4, 0xb42c4, 0xd4330, 0xd4240, 0xd4340, 0xd4440, 0xd4250, 0xd4350, 0xd4450, 0xd4550, 0xd4260, 0xd4360, 0xd4460, 0xd4270, 0xd4370, 0xd4470, 0xd4280, 0xd4380, 0xd4290, 0xd4390, 0xd42a0, 0xd42b0, 0xd42c0, 0xd4334,
	0xd4244, 0xd4344, 0xd4444, 0xd4254, 0xd4354, 0xd4454, 0xd0554, 0xd4264, 0xd4364, 0xd3464, 0xd4274, 0xd4374, 0xc8474, 0xd4284, 0xd3384, 0xd4294, 0xcb394, 0xd42a4, 0xd42b4, 0xd32c4, 0xf4330, 0xf4240, 0xf4340, 0xf4440, 0xf4250, 0xf4350, 0xf4450, 0xf4550, 0xf4260, 0xf4360, 0xf4460, 0xf4270,
	0xf4370, 0xf4280, 0xf4380, 0xf4290, 0xf42a0, 0xf42b0, 0xf42c0, 0xf4334, 0xf4244, 0xf4344, 0xf4444, 0xf4254, 0xf4354, 0xf4454, 0xeb554, 0xf4264, 0xf4364, 0xee464, 0xf4274, 0xf4374, 0xf4284, 0xee384, 0xf4294, 0xf42a4, 0xf32b4, 0xee2c4, 0x114230, 0x114330, 0x114240, 0x114340, 0x114440, 0x114250,
	0x114350, 0x114450, 0x114260, 0x114360, 0x112460, 0x114270, 0x114370, 0x114280, 0x112380, 0x114290, 0x1142a0, 0x1142b0, 0x1122c0, 0x114234, 0x114334, 0x114244, 0x114344, 0x114444, 0x114254, 0x114354, 0x113454, 0x114264, 0x114364, 0x107464, 0x114274, 0x110374, 0x114284, 0x107384, 0x114294, 0x1132a4, 0x10d2b4, 0x1072c4,
	0x134230, 0x134330, 0x134240, 0x134340, 0x134440, 0x134250, 0x134350, 0x134450, 0x134260, 0x134360, 0x134270, 0x134370, 0x134280, 0x134290, 0x1342a0, 0x1322b0, 0x134234, 0x134334, 0x134244, 0x134344, 0x134444, 0x134254, 0x134354, 0x12e454, 0x134264, 0x134364, 0x134274, 0x12b374, 0x134284, 0x134294, 0x12e2a4, 0x1272b4,
	0x154230, 0x154330, 0x154240, 0x154340, 0x154440, 0x154250, 0x154350, 0x154450, 0x154260, 0x154360, 0x154270, 0x154280, 0x154290, 0x1542a0, 0x154234, 0x154334, 0x154244, 0x154344, 0x154444, 0x154254, 0x154354, 0x14a454, 0x154264, 0x151364, 0x154274, 0x154284, 0x151294, 0x14a2a4, 0x174230, 0x174330, 0x174240, 0x174340,
	0x174440, 0x174250, 0x174350, 0x174260, 0x174360, 0x174270, 0x174280, 0x174290, 0x174234, 0x174334, 0x174244, 0x174344, 0x173444, 0x174254, 0x174354, 0x174264, 0x16b364, 0x174274, 0x173284, 0x16b294, 0x214344, 0x214444, 0x214354, 0x214454, 0x214554, 0x214264, 0x214364, 0x214464, 0x214564, 0x214274, 0x214374, 0x214474,
	0x214284, 0x214384, 0x214484, 0x214294, 0x214394, 0x2142a4, 0x2143a4, 0x2142b4, 0x2142c4, 0x234334, 0x234244, 0x234344, 0x234444, 0x234254, 0x234354, 0x234454, 0x231554, 0x234264, 0x234364, 0x234464, 0x225564, 0x234274, 0x234374, 0x22a474, 0x234284, 0x234384, 0x234294, 0x22c394, 0x2342a4, 0x2253a4, 0x2342b4, 0x2342c4,
	0x254234, 0x254334, 0x254244, 0x254344, 0x254444, 0x254254, 0x254354, 0x251454, 0x254264, 0x254364, 0x245464, 0x254274, 0x24e374, 0x254284, 0x245384, 0x254294, 0x2512a4, 0x24b2b4, 0x2452c4, 0x274234, 0x274334, 0x274244, 0x274344, 0x274444, 0x274254, 0x274354, 0x267454, 0x274264, 0x26e364, 0x274274, 0x274284, 0x26e294,
	0x2672a4, 0x294234, 0x294334, 0x294244, 0x294344, 0x28e444, 0x294254, 0x293354, 0x294264, 0x287364, 0x294274, 0x28e284, 0x287294, 0x2b4224, 0x2b4234, 0x2b4334, 0x2b4244, 0x2b4344, 0x2a5444, 0x2b4254, 0x2aa354, 0x2b4264, 0x2ae274, 0x2a5284, 0x2d4224, 0x2d4234, 0x2d4334, 0x2d4244, 0x2d1344, 0x2d4254, 0x2d1264, 0x2c7274,
	0x2f4224, 0x2f4234, 0x2f4334, 0x2f4244, 0x2ec344, 0x2f4254, 0x2ec264, 0x314224, 0x314234, 0x314334, 0x314244, 0x305344, 0x311254, 0x305264, 0x334224, 0x334234, 0x333334, 0x334244, 0x32c254, 0x354224, 0x354234, 0x34f334, 0x354244, 0x348254, 0x374224, 0x374234, 0x36a334, 0x371244
};

enum
{
	cR = 0,
	cG = 1,
	cB = 2,
	cA = 3
};

enum cem_index
{
	cCEM6 = 0,   // RGB Base+Scale
	cCEM8 = 1,   // RGB Direct (also CEM 0 for ranking purposes)
	cCEM10 = 2,  // RGB Base Scale+Two A
	cCEM12 = 3,  // RGBA Direct (also CEM 4 for ranking purposes)

	cCEMTotalIndices
};

static const uint32_t PIXELBUF_ROW_PITCH = 16, PIXELBUF_COMP_PITCH = 16 * 16, PIXELBUF_SIZE_IN_FLOATS = PIXELBUF_COMP_PITCH * 4;

struct pixelbuf
{
	float* m_pBuf;
	uint32_t m_width, m_height;

	inline pixelbuf() {}
	inline pixelbuf(uint32_t width, uint32_t height, float* pBuf) : m_pBuf(pBuf), m_width(width), m_height(height) {}
};

static inline float pixelbuf_get_comp(const float* pPixel_buf, uint32_t x, uint32_t y, uint32_t c)
{
	assert((x + y * PIXELBUF_ROW_PITCH + c * PIXELBUF_COMP_PITCH) < PIXELBUF_SIZE_IN_FLOATS);
	return pPixel_buf[x + y * PIXELBUF_ROW_PITCH + c * PIXELBUF_COMP_PITCH];
}

static inline double pixelbuf_get_comp(const double* pPixel_buf, uint32_t x, uint32_t y, uint32_t c)
{
	assert((x + y * PIXELBUF_ROW_PITCH + c * PIXELBUF_COMP_PITCH) < PIXELBUF_SIZE_IN_FLOATS);
	return pPixel_buf[x + y * PIXELBUF_ROW_PITCH + c * PIXELBUF_COMP_PITCH];
}

static inline float pixelbuf_get_comp(const pixelbuf &pbuf, uint32_t x, uint32_t y, uint32_t c)
{
	assert((x + y * PIXELBUF_ROW_PITCH + c * PIXELBUF_COMP_PITCH) < PIXELBUF_SIZE_IN_FLOATS);
	return pbuf.m_pBuf[x + y * PIXELBUF_ROW_PITCH + c * PIXELBUF_COMP_PITCH];
}

static inline void pixelbuf_set_comp(float* pPixel_buf, uint32_t x, uint32_t y, uint32_t c, float value)
{
	assert((x + y * PIXELBUF_ROW_PITCH + c * PIXELBUF_COMP_PITCH) < PIXELBUF_SIZE_IN_FLOATS);
	pPixel_buf[x + y * PIXELBUF_ROW_PITCH + c * PIXELBUF_COMP_PITCH] = value;
}

static inline void pixelbuf_set_comp(double* pPixel_buf, uint32_t x, uint32_t y, uint32_t c, double value)
{
	assert((x + y * PIXELBUF_ROW_PITCH + c * PIXELBUF_COMP_PITCH) < PIXELBUF_SIZE_IN_FLOATS);
	pPixel_buf[x + y * PIXELBUF_ROW_PITCH + c * PIXELBUF_COMP_PITCH] = value;
}

static inline void pixelbuf_set_comp(const pixelbuf &pbuf, uint32_t x, uint32_t y, uint32_t c, float value)
{
	assert((x + y * PIXELBUF_ROW_PITCH + c * PIXELBUF_COMP_PITCH) < PIXELBUF_SIZE_IN_FLOATS);
	pbuf.m_pBuf[x + y * PIXELBUF_ROW_PITCH + c * PIXELBUF_COMP_PITCH] = value;
}

static inline void pixelbuf_get_pixel(const float* pPixel_buf, uint32_t x, uint32_t y, float* pDst_pixel, uint32_t num_comps)
{
	assert((num_comps == 3) || (num_comps == 4));

	pDst_pixel[0] = pixelbuf_get_comp(pPixel_buf, x, y, cR);
	pDst_pixel[1] = pixelbuf_get_comp(pPixel_buf, x, y, cG);
	pDst_pixel[2] = pixelbuf_get_comp(pPixel_buf, x, y, cB);

	if (num_comps == 4)
		pDst_pixel[3] = pixelbuf_get_comp(pPixel_buf, x, y, cA);
}

static inline void pixelbuf_get_pixel3(const float* pPixel_buf, uint32_t x, uint32_t y, float* pDst_pixel)
{
	pDst_pixel[0] = pixelbuf_get_comp(pPixel_buf, x, y, cR);
	pDst_pixel[1] = pixelbuf_get_comp(pPixel_buf, x, y, cG);
	pDst_pixel[2] = pixelbuf_get_comp(pPixel_buf, x, y, cB);
}

static inline void pixelbuf_get_pixel4(const float* pPixel_buf, uint32_t x, uint32_t y, float* pDst_pixel)
{
	pDst_pixel[0] = pixelbuf_get_comp(pPixel_buf, x, y, cR);
	pDst_pixel[1] = pixelbuf_get_comp(pPixel_buf, x, y, cG);
	pDst_pixel[2] = pixelbuf_get_comp(pPixel_buf, x, y, cB);
	pDst_pixel[3] = pixelbuf_get_comp(pPixel_buf, x, y, cA);
}

static inline void pixelbuf_load_block(pixelbuf &pbuf, const rgba32_image &src_img, uint32_t x_ofs, uint32_t y_ofs, uint32_t num_comps)
{
	const uint32_t width = pbuf.m_width, height = pbuf.m_height;
	float* pDst_pixel_buf = pbuf.m_pBuf;

	assert((num_comps == 3) || (num_comps == 4));
	assert((x_ofs + width) <= src_img.m_width);
	assert((y_ofs + height) <= src_img.m_height);
	assert(src_img.m_width && src_img.m_height && (src_img.m_row_pitch_in_texels >= src_img.m_width));
		
	const uint32_t row_byte_pitch = src_img.m_row_pitch_in_texels * sizeof(uint32_t);

	const uint8_t* pSrc_row = src_img.m_pPixels + ((y_ofs * height) * src_img.m_row_pitch_in_texels + (x_ofs * width)) * sizeof(uint32_t);

	if (num_comps == 3)
	{
		for (uint32_t y = 0; y < height; y++, pSrc_row += row_byte_pitch)
		{
			const uint8_t* pSrc = pSrc_row;

			for (uint32_t x = 0; x < width; x++, pSrc += 4)
			{
				pixelbuf_set_comp(pDst_pixel_buf, x, y, cR, (float)pSrc[0]);
				pixelbuf_set_comp(pDst_pixel_buf, x, y, cG, (float)pSrc[1]);
				pixelbuf_set_comp(pDst_pixel_buf, x, y, cB, (float)pSrc[2]);
				pixelbuf_set_comp(pDst_pixel_buf, x, y, cA, 255.0f);
			}
		}
	}
	else
	{
		for (uint32_t y = 0; y < height; y++, pSrc_row += row_byte_pitch)
		{
			const uint8_t* pSrc = pSrc_row;

			for (uint32_t x = 0; x < width; x++, pSrc += 4)
			{
				pixelbuf_set_comp(pDst_pixel_buf, x, y, cR, (float)pSrc[0]);
				pixelbuf_set_comp(pDst_pixel_buf, x, y, cG, (float)pSrc[1]);
				pixelbuf_set_comp(pDst_pixel_buf, x, y, cB, (float)pSrc[2]);
				pixelbuf_set_comp(pDst_pixel_buf, x, y, cA, (float)pSrc[3]);
			}
		}
	}
}

static void pixelbuf_set_comp_to_val(pixelbuf &pbuf, uint32_t comp, float val)
{
	const uint32_t width = pbuf.m_width, height = pbuf.m_height;
	float* pDst_pixel_buf = pbuf.m_pBuf;

	for (uint32_t y = 0; y < height; y++)
		for (uint32_t x = 0; x < width; x++)
			pixelbuf_set_comp(pDst_pixel_buf, x, y, comp, val);
}

static void pixelbuf_swap_comp_with_alpha(pixelbuf &pbuf, uint32_t comp)
{
	const uint32_t width = pbuf.m_width, height = pbuf.m_height;
	float* pDst_pixel_buf = pbuf.m_pBuf;

	assert((comp >= 0) && (comp <= 3));

	if (comp == cR)
	{
		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				const float r = pixelbuf_get_comp(pDst_pixel_buf, x, y, cR);
				const float a = pixelbuf_get_comp(pDst_pixel_buf, x, y, cA);
				pixelbuf_set_comp(pDst_pixel_buf, x, y, cR, a);
				pixelbuf_set_comp(pDst_pixel_buf, x, y, cA, r);
			}
		}
	}
	else if (comp == cG)
	{
		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				const float g = pixelbuf_get_comp(pDst_pixel_buf, x, y, cG);
				const float a = pixelbuf_get_comp(pDst_pixel_buf, x, y, cA);
				pixelbuf_set_comp(pDst_pixel_buf, x, y, cG, a);
				pixelbuf_set_comp(pDst_pixel_buf, x, y, cA, g);
			}
		}
	}
	else if (comp == cB)
	{
		for (uint32_t y = 0; y < height; y++)
		{
			for (uint32_t x = 0; x < width; x++)
			{
				const float b = pixelbuf_get_comp(pDst_pixel_buf, x, y, cB);
				const float a = pixelbuf_get_comp(pDst_pixel_buf, x, y, cA);
				pixelbuf_set_comp(pDst_pixel_buf, x, y, cB, a);
				pixelbuf_set_comp(pDst_pixel_buf, x, y, cA, b);
			}
		}
	}
}

static inline bool does_cem_have_alpha(uint32_t cem)
{
	return (cem == 4) || (cem >= 10);
}

static inline uint32_t get_num_cem_chans(uint32_t cem)
{
	return ((cem == 4) || (cem >= 10)) ? 4 : 3;
}

// l and la
[[maybe_unused]] static inline bool is_cem_0_or_4(uint32_t cem)
{
	return (cem == 0) || (cem == 4);
}

// base+scale and base+scale plus 2 a
static inline bool is_cem_6_or_10(uint32_t cem)
{
	return (cem == 6) || (cem == 10);
}

// rgb(a) direct
static inline bool is_cem_8_or_12(uint32_t cem)
{
	return (cem == 8) || (cem == 12);
}

// rgb(a) base+ofs
static inline bool is_cem_9_or_13(uint32_t cem)
{
	return (cem == 9) || (cem == 13);
}

void convert_rank_lblock_to_ise(astc_helpers::log_astc_block& log_blk)
{
	if ((log_blk.m_solid_color_flag_ldr) || (is_lblock_ise(log_blk)))
		return;

	const auto& endpoint_tab = astc_helpers::g_dequant_tables.get_endpoint_tab(log_blk.m_endpoint_ise_range).m_rank_to_ISE;

	const uint32_t num_endpoint_vals = astc_helpers::get_total_endpoint_vals(log_blk);
	for (uint32_t i = 0; i < num_endpoint_vals; i++)
		log_blk.m_endpoints[i] = (uint8_t)endpoint_tab[log_blk.m_endpoints[i]];

	const auto& weight_tab = astc_helpers::g_dequant_tables.get_weight_tab(log_blk.m_weight_ise_range).m_rank_to_ISE;

	const uint32_t num_weight_vals = astc_helpers::get_total_weights(log_blk);
	for (uint32_t i = 0; i < num_weight_vals; i++)
		log_blk.m_weights[i] = (uint8_t)weight_tab[log_blk.m_weights[i]];

	log_blk.m_user_mode = (uint8_t)cUserModeISEValues;
}

void convert_ise_lblock_to_rank(astc_helpers::log_astc_block& log_blk)
{
	if ((log_blk.m_solid_color_flag_ldr) || (!is_lblock_ise(log_blk)))
		return;

	const auto& endpoint_tab = astc_helpers::g_dequant_tables.get_endpoint_tab(log_blk.m_endpoint_ise_range).m_ISE_to_rank;

	const uint32_t num_endpoint_vals = astc_helpers::get_total_endpoint_vals(log_blk);
	for (uint32_t i = 0; i < num_endpoint_vals; i++)
		log_blk.m_endpoints[i] = (uint8_t)endpoint_tab[log_blk.m_endpoints[i]];

	const auto& weight_tab = astc_helpers::g_dequant_tables.get_weight_tab(log_blk.m_weight_ise_range).m_ISE_to_rank;

	const uint32_t num_weight_vals = astc_helpers::get_total_weights(log_blk);
	for (uint32_t i = 0; i < num_weight_vals; i++)
		log_blk.m_weights[i] = (uint8_t)weight_tab[log_blk.m_weights[i]];

	log_blk.m_user_mode = (uint8_t)cUserModeRankValues;
}

static double compute_block_error(const astc_helpers::log_astc_block& lblk, const pixelbuf& src_block, const single_subset_enc_context& ctx)
{
	const astc_helpers::log_astc_block* pBlock = &lblk;

	astc_helpers::log_astc_block lblk_temp;

	if (!is_lblock_ise(lblk))
	{
		lblk_temp = lblk;
		convert_rank_lblock_to_ise(lblk_temp);
		pBlock = &lblk_temp;
	}

	astc_helpers::color_rgba block_pixels[astc_helpers::MAX_BLOCK_PIXELS];
	bool status = astc_helpers::decode_block_xuastc_ldr(*pBlock, block_pixels, ctx.m_block_width, ctx.m_block_height, ctx.m_astc_decode_mode);
	assert(status);
	if (!status)
		return DBL_MAX;

	const float wr = (float)ctx.m_chan_weights[0], wg = (float)ctx.m_chan_weights[1], wb = (float)ctx.m_chan_weights[2], wa = (float)ctx.m_chan_weights[3];

	double wsse = 0;

	const astc_helpers::color_rgba* pBlock_pixel = block_pixels;

	for (uint32_t y = 0; y < ctx.m_block_height; y++)
	{
		for (uint32_t x = 0; x < ctx.m_block_width; x++)
		{
			wsse += squaref((float)pBlock_pixel->m_r - pixelbuf_get_comp(src_block.m_pBuf, x, y, 0)) * wr;
			wsse += squaref((float)pBlock_pixel->m_g - pixelbuf_get_comp(src_block.m_pBuf, x, y, 1)) * wg;
			wsse += squaref((float)pBlock_pixel->m_b - pixelbuf_get_comp(src_block.m_pBuf, x, y, 2)) * wb;
			wsse += squaref((float)pBlock_pixel->m_a - pixelbuf_get_comp(src_block.m_pBuf, x, y, 3)) * wa;

			++pBlock_pixel;
		}
	}

	return wsse;
}

// Scatter/covariance matrix values
// Symmetric matrix form:
// 0 1 2 3
// 1 4 5 6
// 2 5 7 8
// 3 6 8 9
enum
{
	cCovarRR = 0, cCovarRG = 1, cCovarRB = 2, cCovarRA = 3,
	              cCovarGG = 4, cCovarGB = 5, cCovarGA = 6,
	                            cCovarBB = 7, cCovarBA = 8,
	                                          cCovarAA = 9, 
	cTotalCovar = 10,
};

// Columns of the symmetric 4x4 covariance matrix
static const uint8_t s_covar_col_indices[4][4] =
{
	{ cCovarRR, cCovarRG, cCovarRB, cCovarRA },
	{ cCovarRG, cCovarGG, cCovarGB, cCovarGA },
	{ cCovarRB, cCovarGB, cCovarBB, cCovarBA },
	{ cCovarRA, cCovarGA, cCovarBA, cCovarAA }
};

// Channel pair Pearson correlation coefficients
enum
{
	cCorrRG = 0, cCorrRB = 1, cCorrRA = 2,
	             cCorrGB = 3, cCorrGA = 4,
	                          cCorrBA = 5,
	cTotalCorr = 6
};

static void compute_block_moments(
	float pMoments[cTotalCovar], float pSums[4],
	const pixelbuf& pixel_buf, 
	uint32_t num_comps, 
	bool zero_centered)
{
	assert((num_comps == 3) || (num_comps == 4));

	if (zero_centered)
	{
		if (num_comps == 3)
		{
			for (uint32_t y = 0; y < pixel_buf.m_height; y++)
			{
				for (uint32_t x = 0; x < pixel_buf.m_width; x++)
				{
					const float r = pixelbuf_get_comp(pixel_buf.m_pBuf, x, y, cR);
					const float g = pixelbuf_get_comp(pixel_buf.m_pBuf, x, y, cG);
					const float b = pixelbuf_get_comp(pixel_buf.m_pBuf, x, y, cB);

					pMoments[cCovarRR] += r * r; pMoments[cCovarRG] += r * g; pMoments[cCovarRB] += r * b;
					pMoments[cCovarGG] += g * g; pMoments[cCovarGB] += g * b;
					pMoments[cCovarBB] += b * b;
				} // x
			} // y
		}
		else
		{
			assert(num_comps == 4);

			for (uint32_t y = 0; y < pixel_buf.m_height; y++)
			{
				for (uint32_t x = 0; x < pixel_buf.m_width; x++)
				{
					const float r = pixelbuf_get_comp(pixel_buf.m_pBuf, x, y, cR);
					const float g = pixelbuf_get_comp(pixel_buf.m_pBuf, x, y, cG);
					const float b = pixelbuf_get_comp(pixel_buf.m_pBuf, x, y, cB);
					const float a = pixelbuf_get_comp(pixel_buf.m_pBuf, x, y, cA);

					pMoments[cCovarRR] += r * r; pMoments[cCovarRG] += r * g; pMoments[cCovarRB] += r * b;
					pMoments[cCovarGG] += g * g; pMoments[cCovarGB] += g * b;
					pMoments[cCovarBB] += b * b;

					pMoments[cCovarRA] += r * a; pMoments[cCovarGA] += g * a; pMoments[cCovarBA] += b * a; pMoments[cCovarAA] += a * a;
				} // x
			} // y
		}
	}
	else
	{
		if (num_comps == 3)
		{
			for (uint32_t y = 0; y < pixel_buf.m_height; y++)
			{
				for (uint32_t x = 0; x < pixel_buf.m_width; x++)
				{
					const float r = pixelbuf_get_comp(pixel_buf.m_pBuf, x, y, cR);
					const float g = pixelbuf_get_comp(pixel_buf.m_pBuf, x, y, cG);
					const float b = pixelbuf_get_comp(pixel_buf.m_pBuf, x, y, cB);

					pSums[cR] += r; pSums[cG] += g; pSums[cB] += b;

					pMoments[cCovarRR] += r * r; pMoments[cCovarRG] += r * g; pMoments[cCovarRB] += r * b;
					pMoments[cCovarGG] += g * g; pMoments[cCovarGB] += g * b;
					pMoments[cCovarBB] += b * b;
				} // x
			} // y
		}
		else
		{
			assert(num_comps == 4);

			for (uint32_t y = 0; y < pixel_buf.m_height; y++)
			{
				for (uint32_t x = 0; x < pixel_buf.m_width; x++)
				{
					const float r = pixelbuf_get_comp(pixel_buf.m_pBuf, x, y, cR);
					const float g = pixelbuf_get_comp(pixel_buf.m_pBuf, x, y, cG);
					const float b = pixelbuf_get_comp(pixel_buf.m_pBuf, x, y, cB);
					const float a = pixelbuf_get_comp(pixel_buf.m_pBuf, x, y, cA);

					pSums[cR] += r; pSums[cG] += g; pSums[cB] += b; pSums[cA] += a;

					pMoments[cCovarRR] += r * r; pMoments[cCovarRG] += r * g; pMoments[cCovarRB] += r * b;
					pMoments[cCovarGG] += g * g; pMoments[cCovarGB] += g * b;
					pMoments[cCovarBB] += b * b;

					pMoments[cCovarRA] += r * a; pMoments[cCovarGA] += g * a; pMoments[cCovarBA] += b * a; pMoments[cCovarAA] += a * a;
				} // x
			} // y
		}
	}
}

static inline void calc_unnormalized_covariance(
	const float pMoments[cTotalCovar], const float pSums[4], 
	float pDst_covar[cTotalCovar], 
	uint32_t num_comps, 
	uint32_t total_pixels, 
	bool zero_centered)
{
	assert((num_comps == 3) || (num_comps == 4));
	assert(total_pixels > 0);

	if (zero_centered)
	{
		memcpy(pDst_covar, pMoments, sizeof(float) * cTotalCovar);
		return;
	}
	
	const float oo_total_texels = 1.0f / (float)total_pixels;

	const float sum_r = pSums[cR], sum_g = pSums[cG], sum_b = pSums[cB];

	pDst_covar[cCovarRR] = pMoments[cCovarRR] - (sum_r * sum_r * oo_total_texels);
	pDst_covar[cCovarRG] = pMoments[cCovarRG] - (sum_r * sum_g * oo_total_texels);
	pDst_covar[cCovarRB] = pMoments[cCovarRB] - (sum_r * sum_b * oo_total_texels);
	pDst_covar[cCovarGG] = pMoments[cCovarGG] - (sum_g * sum_g * oo_total_texels);
	pDst_covar[cCovarGB] = pMoments[cCovarGB] - (sum_g * sum_b * oo_total_texels);
	pDst_covar[cCovarBB] = pMoments[cCovarBB] - (sum_b * sum_b * oo_total_texels);
	
	pDst_covar[cCovarRA] = 0; pDst_covar[cCovarGA] = 0; pDst_covar[cCovarBA] = 0; pDst_covar[cCovarAA] = 0;
	if (num_comps == 4)
	{
		const float sum_a = pSums[cA];
		pDst_covar[cCovarRA] = pMoments[cCovarRA] - (sum_r * sum_a * oo_total_texels);
		pDst_covar[cCovarGA] = pMoments[cCovarGA] - (sum_g * sum_a * oo_total_texels);
		pDst_covar[cCovarBA] = pMoments[cCovarBA] - (sum_b * sum_a * oo_total_texels);
		pDst_covar[cCovarAA] = pMoments[cCovarAA] - (sum_a * sum_a * oo_total_texels);
	}
}

// corr [-1,1]: 0=RG, 1=RB, 2=RA, 3=GB, 4=GA, 5=BA
// note not clamped
// xy = cross term
// xx = first chan
// xy = second chan
static inline float corr_pair(float xy, float xx, float yy)
{
	float d = xx * yy;
	if (d <= TINY_EPS)
		return 1.0f; // one or both channels aren't active; returning 1.0f, not 0.0f, to simplify channel correlation checks later

	float r = xy / sqrtf(d); // note not clamped
	return r;
}

// Pearson correlation coefficients (assumed mean removed covar)
static inline void corr_from_covar(float corr[6], const float covar[10], uint32_t num_comps)
{
	assert((num_comps == 3) || (num_comps == 4));

	corr[cCorrRG] = corr_pair(covar[cCovarRG], covar[cCovarRR], covar[cCovarGG]); // RG
	corr[cCorrRB] = corr_pair(covar[cCovarRB], covar[cCovarRR], covar[cCovarBB]); // RB
	corr[cCorrGB] = corr_pair(covar[cCovarGB], covar[cCovarGG], covar[cCovarBB]); // GB

	corr[cCorrRA] = corr[cCorrGA] = corr[cCorrBA] = 1.0f; // chan pairs with A default to 1.0f, not 0

	if (num_comps == 4)
	{
		corr[cCorrRA] = corr_pair(covar[cCovarRA], covar[cCovarRR], covar[cCovarAA]); // RA
		corr[cCorrGA] = corr_pair(covar[cCovarGA], covar[cCovarGG], covar[cCovarAA]); // GA
		corr[cCorrBA] = corr_pair(covar[cCovarBA], covar[cCovarBB], covar[cCovarAA]); // BA
	}
}

// computes unnormalized mean or zero centered 4D covar (really scatter) and block mean (covar matrix elements are NOT divided by the total # of texels in the block)
// mean will be 0 if zero_centered is true
static inline void compute_covariance(float pDst_covar[cTotalCovar], float mean[4], const pixelbuf& block, uint32_t num_comps, bool zero_centered)
{
	assert((num_comps == 3) || (num_comps == 4));

	float moments[cTotalCovar];
	
	memset(moments, 0, sizeof(moments));
	memset(mean, 0, sizeof(float) * 4);

	compute_block_moments(moments, mean, block, num_comps, zero_centered);
	
	const uint32_t total_pixels = block.m_width * block.m_height;
	calc_unnormalized_covariance(moments, mean, pDst_covar, num_comps, total_pixels, zero_centered);

	if (!zero_centered)
	{
		const float one_over_total = 1.0f / (float)total_pixels;
		for (uint32_t c = 0; c < num_comps; c++)
			mean[c] *= one_over_total;
	}
}

// 4x4 * 4x1 = 4x1
// Computes covar_matrix * vec4.
static inline void covar_mul_vec4(float pDst[4], const float pCovar[cTotalCovar], const float pSrc[4])
{
	const float x = pSrc[0], y = pSrc[1], z = pSrc[2], w = pSrc[3];

	pDst[0] = pCovar[cCovarRR] * x + pCovar[cCovarRG] * y + pCovar[cCovarRB] * z + pCovar[cCovarRA] * w;
	pDst[1] = pCovar[cCovarRG] * x + pCovar[cCovarGG] * y + pCovar[cCovarGB] * z + pCovar[cCovarGA] * w;
	pDst[2] = pCovar[cCovarRB] * x + pCovar[cCovarGB] * y + pCovar[cCovarBB] * z + pCovar[cCovarBA] * w;
	pDst[3] = pCovar[cCovarRA] * x + pCovar[cCovarGA] * y + pCovar[cCovarBA] * z + pCovar[cCovarAA] * w;
}

// 3x3 * 3x1 = 3x1
// Computes covar_matrix * vec3.
// pDst[3] is explicitly cleared so callers can safely treat pDst as vec4.
static inline void covar_mul_vec3(float pDst[4], const float pCovar[cTotalCovar], const float pSrc[4])
{
	const float x = pSrc[0], y = pSrc[1], z = pSrc[2];

	pDst[0] = pCovar[cCovarRR] * x + pCovar[cCovarRG] * y + pCovar[cCovarRB] * z;
	pDst[1] = pCovar[cCovarRG] * x + pCovar[cCovarGG] * y + pCovar[cCovarGB] * z;
	pDst[2] = pCovar[cCovarRB] * x + pCovar[cCovarGB] * y + pCovar[cCovarBB] * z;
	pDst[3] = 0.0f;
}

static inline void get_initial_axis_from_largest_diag(float v[4], const float covar[cTotalCovar], uint32_t num_comps)
{
	assert((num_comps == 3) || (num_comps == 4));
		
	[[maybe_unused]] static const uint8_t s_diag_indices[4] = { cCovarRR, cCovarGG, cCovarBB, cCovarAA };

	uint32_t best_col = 0;
	float best_diag = covar[cCovarRR];

	if (covar[cCovarGG] > best_diag)
	{
		best_diag = covar[cCovarGG];
		best_col = 1;
	}

	if (covar[cCovarBB] > best_diag)
	{
		best_diag = covar[cCovarBB];
		best_col = 2;
	}

	if ((num_comps == 4) && (covar[cCovarAA] > best_diag))
	{
		best_diag = covar[cCovarAA];
		best_col = 3;
	}

	const uint8_t* pCol = s_covar_col_indices[best_col];

	v[0] = covar[pCol[0]];
	v[1] = covar[pCol[1]];
	v[2] = covar[pCol[2]];
	v[3] = (num_comps == 4) ? covar[pCol[3]] : 0.0f;
}

// all 4 elements of pAxis set
static void compute_principle_axis(float pAxis[4], const float pCovar[cTotalCovar], uint32_t max_pair_iters, uint32_t num_comps)
{
	assert(max_pair_iters);
	assert((num_comps == 3) || (num_comps == 4));

	//vec4_set(pAxis, 1.0f, 1.0f, 1.0f, (num_comps == 4) ? 1.0f : 0.0f);
	get_initial_axis_from_largest_diag(pAxis, pCovar, num_comps);

	//vec_normalize(pAxis, num_comps);
	float s = maximum(fabs(pAxis[0]), fabs(pAxis[1]), fabs(pAxis[2]), fabs(pAxis[3]));
	if (s > TINY_EPS)
		vec4_scale(pAxis, 1.0f / s);

	float temp_vec[4], prev_axis[4];

	for (uint32_t i = 0; i < max_pair_iters; ++i)
	{
		vec_copy(prev_axis, pAxis, 4);

		if (num_comps == 3)
		{
			covar_mul_vec3(temp_vec, pCovar, pAxis);
			covar_mul_vec3(pAxis, pCovar, temp_vec);
		}
		else
		{
			covar_mul_vec4(temp_vec, pCovar, pAxis);
			covar_mul_vec4(pAxis, pCovar, temp_vec);
		}
				
		const float total_sq = vec_normalize(pAxis, num_comps);

		assert(!std::isinf(total_sq));
		
		if (total_sq < TINY_EPS)
		{
			// should be very rare
			vec4_set(pAxis, 1.0f, 1.0f, 1.0f, (num_comps == 4) ? 1.0f : 0.0f);
			vec_normalize(pAxis, num_comps);
			break;
		}

		if (i)
		{
			float d = vec4_dot(pAxis, prev_axis);
			const float DOT_THRESH = 0.9997f;
			if (d >= DOT_THRESH)
				break;
		}
	}
}

struct block_stats
{
	float m_covar[cTotalCovar]; // unnormalized covar
	float m_corr[cTotalCorr]; // Pearson's but unclamped, defaults to 1.0 (see corr_pair())
	float m_mean[4];
	float m_axis[4]; // 3D or 4D normalized
};

// endpoint values returned in direct ASTC endpoint order: lr hr lg hg lb hb la ha
static void calc_initial_cem_endpoints(const pixelbuf& block, float pCEM_values[8], uint32_t num_comps, block_stats *pOut_stats, bool zero_centered)
{
	//static inline void compute_covariance(float pDst_covar[cTotalCovar], float* pMean, const pixelbuf & block, uint32_t num_comps, bool zero_centered)
	float cov[cTotalCovar];
	float block_mean[4];

	compute_covariance(cov, block_mean, block, num_comps, zero_centered);

	if (pOut_stats)
	{
		memcpy(pOut_stats->m_covar, cov, sizeof(float) * cTotalCovar);

		corr_from_covar(pOut_stats->m_corr, cov, num_comps);

		// will be 0 if zero_centered
		memcpy(pOut_stats->m_mean, block_mean, sizeof(float) * 4);
	}
	
	cov[cCovarRR] += SMALL_EPS;
	cov[cCovarGG] += SMALL_EPS;
	cov[cCovarBB] += SMALL_EPS;
	if (num_comps == 4)
		cov[cCovarAA] += SMALL_EPS;

	float axis[4];
	
	const uint32_t MAX_POWER_ITER_PAIRS = 5;
	compute_principle_axis(axis, cov, MAX_POWER_ITER_PAIRS, num_comps);

	if (pOut_stats)
		memcpy(pOut_stats->m_axis, axis, sizeof(float) * 4);

	float span[2] = { 1e+30f, -1e+30f };
	for (uint32_t y = 0; y < block.m_height; y++)
	{
		for (uint32_t x = 0; x < block.m_width; x++)
		{
			float d = 0;
			for (uint32_t c = 0; c < num_comps; c++)
				d += (pixelbuf_get_comp(block.m_pBuf, x, y, c) - block_mean[c]) * axis[c];

			span[0] = basisu::minimum(span[0], d);
			span[1] = basisu::maximum(span[1], d);
		}
	}

	// degenerate span check
	if ((span[0] + 1.0f) > span[1])
	{
		span[0] -= 0.5f;
		span[1] += 0.5f;
	}

	for (uint32_t e = 0; e < 2; e++)
	{
		for (uint32_t c = 0; c < num_comps; c++)
			pCEM_values[c * 2 + e] = clamp<float>(axis[c] * span[e] + block_mean[c], 0.0f, 255.0f);

		if (num_comps == 3)
		{
			pCEM_values[3 * 2 + 0] = 0;
			pCEM_values[3 * 2 + 1] = 255;
		}
	}
}

// 8 bytes - used for shortlist generation
struct astc_unpacked_config
{
	uint8_t m_cem;
	uint8_t m_grid_width;
	uint8_t m_grid_height;
	uint8_t m_endpoint_range;
	
	uint8_t m_weight_range;
	uint8_t m_dual_plane;
	uint8_t m_ccs_index;
	uint8_t m_unused;
};

struct single_subset_shortlist_state
{
	uint32_t m_block_width, m_block_height;
	uint32_t m_max_candidates;

	uint32_t m_num_src_block_comps; // whether the original src block has 3 or 4 active components
	bool m_src_is_luma_only;
	bool m_should_include_dual_plane;

	pixelbuf m_pbuf;

	float m_block_pixels[PIXELBUF_SIZE_IN_FLOATS];

	// 2x2 to 12x12, [h - 2][w - 2], lost high frequency pixel energy (SSE) for RGBA
	float m_downsample_sse[11][11];

	// 0=CEM  6: RGB Base+Scale       (3D PCA, zero RGB mean, A=255), CCS can be [0,2]
	// 1=CEM  8: RGB Direct           (3D PCA, A=255), CCS can be [0,2]
	// 2=CEM 10: RGB Base+Scale+Two A (3D PCA, zero RGB mean, A=direct), CCS can be [0,3]
	// 3=CEM 12: RGBA Direct          (4D PCA, A=direct), CCS can be [0,3]

	// Single Plane
	// [CEM index][chan]
	float m_sp_spans[cCEMTotalIndices][4];
	float m_sp_slam_to_line_error[cCEMTotalIndices];
	bool m_sp_valid[cCEMTotalIndices];

	// Dual Plane
	// [CEM index][CCS Index][chan]
	float m_dp_spans[cCEMTotalIndices][4][4];
	float m_dp_slam_to_line_error[cCEMTotalIndices][4];
	bool m_dp_valid[cCEMTotalIndices][4];

	// mean centered block stats
	// unclamped Pearson, order RG, RB, GB, RA, GA, BA; beware if a channel is invalid it's 1 not 0
	// normalized 3D or 4D (depending on if the block had alpha or not)
	block_stats m_stats;

	float m_best_sse[MAX_CANDIDATES];
	astc_unpacked_config m_best_configs[MAX_CANDIDATES];
};

static void fit_and_measure(
	const pixelbuf &block,
	uint32_t num_comps,				// 3 (RGB only) or 4 (RGBA)
	bool zero_centered,				// true => line through origin
	float pOut_span[4],				// written for [0..num_chans-1]; rest untouched
	float* pOut_ortho_error,		// slam to line error
	block_stats *pOut_stats,
	float pOut_ortho_error_rgba[4]) // per-channel slam to line error (only num_comps channels valid)
{
	float endpoints[8];
	calc_initial_cem_endpoints(block, endpoints, num_comps, pOut_stats, zero_centered);

	pOut_span[3] = 0.0f;

	float endpoint_org[4] = { 0, 0, 0, 0 };
	float endpoint_dir[4] = { 0, 0, 0, 0 };
	for (uint32_t c = 0; c < num_comps; c++)
	{
		endpoint_org[c] = endpoints[c * 2 + 0];
		endpoint_dir[c] = endpoints[c * 2 + 1] - endpoints[c * 2 + 0];
		pOut_span[c] = endpoint_dir[c];  // signed; squared in quant formula
	}

	const float inv_dir_sq_len = 1.0f / (vec_get_squared_len(endpoint_dir, num_comps) + TINY_EPS);

	float total_ortho_error_c[4] = { };

	for (uint32_t y = 0; y < block.m_height; y++)
	{
		for (uint32_t x = 0; x < block.m_width; x++)
		{
			float d = 0.0f;
			for (uint32_t c = 0; c < num_comps; c++)
			{
				float v = pixelbuf_get_comp(block.m_pBuf, x, y, c);
				d += (v - endpoint_org[c]) * endpoint_dir[c];
			}

			const float dist_along = d * inv_dir_sq_len;
						
			for (uint32_t c = 0; c < num_comps; c++)
			{
				const float block_val = pixelbuf_get_comp(block.m_pBuf, x, y, c);
				const float nearest_val = endpoint_dir[c] * dist_along + endpoint_org[c];
				total_ortho_error_c[c] += square(block_val - nearest_val);
			}
		}
	}

	float total_ortho_error = 0;

	for (uint32_t c = 0; c < num_comps; c++)
		total_ortho_error += total_ortho_error_c[c];

	*pOut_ortho_error = total_ortho_error;

	if (pOut_ortho_error_rgba)
		vec4_copy(pOut_ortho_error_rgba, total_ortho_error_c);
}

static void fit_and_measure_2D(
	const pixelbuf& block, uint32_t chan_to_zero,
	bool zero_centered,				// true => line through origin
	float pOut_span[4],				// written for [0..num_chans-1]; rest untouched
	float* pOut_ortho_error,
	block_stats* pOut_stats,
	float *pOut_ortho_error_rgba)
{
	float temp_pixels[PIXELBUF_SIZE_IN_FLOATS];
	memcpy(temp_pixels, block.m_pBuf, PIXELBUF_SIZE_IN_FLOATS * sizeof(float));

	pixelbuf temp_pbuf(block);
	temp_pbuf.m_pBuf = temp_pixels;
	
	pixelbuf_set_comp_to_val(temp_pbuf, chan_to_zero, 0.0f);

	const int num_chans = 3;
	return fit_and_measure(temp_pbuf, num_chans, zero_centered, pOut_span, pOut_ortho_error, pOut_stats, pOut_ortho_error_rgba);
}

static void fit_and_measure_1D(const pixelbuf &block, uint32_t comp_index, float* pOut_span, float* pOut_slam_to_255_sse)
{
	float lo = 1e+30f, hi = -1e+30f, a_err = 0.0f;

	for (uint32_t y = 0; y < block.m_height; y++)
	{
		for (uint32_t x = 0; x < block.m_width; x++)
		{
			const float v = pixelbuf_get_comp(block.m_pBuf, x, y, comp_index);

			lo = basisu::minimum(lo, v);
			hi = basisu::maximum(hi, v);

			a_err += square(v - 255.0f);
		}
	}

	if (pOut_span)
		*pOut_span = hi - lo;

	if (pOut_slam_to_255_sse)
		*pOut_slam_to_255_sse = a_err;
}

#if 0
static float compute_ac_energy_from_dct(uint32_t block_width, uint32_t block_height, float* pDCT)
{
	pixel
	float total_energy = 0.0f;
	for (uint32_t i = 1; i < num_texels; i++)
	{
		const float v = square(pDCT[i]);
		pDCT[i] = v;
		total_energy += v;
	}

	pDCT[0] = 0.0f;
	return total_energy;
}
#endif

#define BASISU_USE_ENERGY_PREFIX_SUM (1)
typedef double prefix_sum_t; // probably not really necessary, but due to the amount of summation involved double is useful here (and still way faster than not using a prefix sum table)

// Build 2D prefix sums over a row-major block of energies.
// prefix[x + y * block_width] contains the sum over [0..x] x [0..y], inclusive.
// Slams DC energy to 0, returns total_ac_energy.
static inline prefix_sum_t prepare_dct_energy_prefix_table(uint32_t block_width, uint32_t block_height, float* pEnergy, prefix_sum_t* pPrefix)
{
	pixelbuf_set_comp(pEnergy, 0, 0, 0, 0.0f); // ignore DC

	prefix_sum_t total_ac_energy = 0.0f;

	for (uint32_t y = 0; y < block_height; y++)
	{
		prefix_sum_t row_sum = 0.0f;

		for (uint32_t x = 0; x < block_width; x++)
		{
			const float ac_energy = pixelbuf_get_comp(pEnergy, x, y, 0);
			
			total_ac_energy += ac_energy;

			row_sum += ac_energy;

			const prefix_sum_t val_above = y ? pixelbuf_get_comp(pPrefix, x, y - 1, 0) : 0.0f;

			pixelbuf_set_comp(pPrefix, x, y, 0, row_sum + val_above);
		} // x
	} // y

	return total_ac_energy;
}

// Sum of the origin-anchored rectangle [0, grid_w) x [0, grid_h).
static inline prefix_sum_t query_dct_energy_prefix_sum(uint32_t block_width, uint32_t block_height, const prefix_sum_t* pPrefix, uint32_t grid_w, uint32_t grid_h)
{
	BASISU_NOTE_UNUSED(block_width);
	BASISU_NOTE_UNUSED(block_height);

	assert((grid_w >= 1) && (grid_w <= block_width));
	assert((grid_h >= 1) && (grid_h <= block_height));

	return pixelbuf_get_comp(pPrefix, grid_w - 1, grid_h - 1, 0);
}

static inline prefix_sum_t compute_lost_dct_energy_prefix_sum(uint32_t block_width, uint32_t block_height, const prefix_sum_t* pEnergy_prefix_sum, uint32_t grid_w, uint32_t grid_h, prefix_sum_t total_ac_energy)
{
	const prefix_sum_t kept_energy = query_dct_energy_prefix_sum(block_width, block_height, pEnergy_prefix_sum, grid_w, grid_h);

	return maximum<prefix_sum_t>(total_ac_energy - kept_energy, 0.0f);
}

static bool should_include_dual_plane(const single_subset_shortlist_state& shortlist_state, bool has_a)
{
	const block_stats& stats = shortlist_state.m_stats;

	const float var_r = stats.m_covar[cCovarRR], var_g = stats.m_covar[cCovarGG], var_b = stats.m_covar[cCovarBB];
	const bool has_r = (var_r != 0.0f), has_g = (var_g != 0.0f), has_b = (var_b != 0.0f);

	const uint32_t total_active_chans = has_r + has_g + has_b + has_a;

	if (total_active_chans <= 1)
		return false;

	//const float MIN_A_DP_CORR = .995f;
	//const float MIN_RGB_DP_CORR = .985f;

	const float MIN_A_DP_CORR = .9999f;
	const float MIN_RGB_DP_CORR = .9999f;

	if (has_a)
	{
		float min_corr_vs_a = basisu::minimum(basisu::minimum(fabsf(stats.m_corr[cCorrRA]), fabsf(stats.m_corr[cCorrGA])), fabsf(stats.m_corr[cCorrBA]));
		if (min_corr_vs_a < MIN_A_DP_CORR)
			return true;
	}

	const float rg_corr = fabs(stats.m_corr[cCorrRG]);
	const float rb_corr = fabs(stats.m_corr[cCorrRB]);
	const float gb_corr = fabs(stats.m_corr[cCorrGB]);

	float min_p = basisu::minimum(rg_corr, rb_corr, gb_corr);

	if (min_p < MIN_RGB_DP_CORR)
		return true;

	return false;
}

// num_comps==4 if block has alpha and cems 4/10/12 are valid
// if num_comps=3 only stats for cem 0/6/8 are valid (if the block doesn't have alpha, it makes no sense to use cem 4/10/12)
static void compute_block_metrics(single_subset_shortlist_state&state, uint32_t num_comps, const basist::astc_ldr_t::dct2f &dct, const single_subset_enc_context &ctx)
{
	float temp_buf[PIXELBUF_SIZE_IN_FLOATS];
	memcpy(temp_buf, state.m_block_pixels, PIXELBUF_SIZE_IN_FLOATS * sizeof(float));
	
	pixelbuf temp_pbuf(state.m_block_width, state.m_block_height, temp_buf);

	memset(state.m_sp_spans, 0, sizeof(state.m_sp_spans));
	memset(state.m_sp_slam_to_line_error, 0, sizeof(state.m_sp_slam_to_line_error));
	memset(state.m_sp_valid, 0, sizeof(state.m_sp_valid));

	memset(state.m_dp_spans, 0, sizeof(state.m_dp_spans));
	memset(state.m_dp_slam_to_line_error, 0, sizeof(state.m_dp_slam_to_line_error));
	memset(state.m_dp_valid, 0, sizeof(state.m_dp_valid));

	float chan_ranges[4], a_slam_to_255_sse = 0;

	for (uint32_t c = 0; c < 4; c++)
		fit_and_measure_1D(temp_pbuf, c, &chan_ranges[c], (c == 3) ? &a_slam_to_255_sse : nullptr);

	// Single Plane
	float cem6_ortho_error_rgba[4] = { };
	float cem10_ortho_error_rgba[4] = { };

	// CEM 6: RGB Base+Scale, A=always 255
	fit_and_measure(temp_pbuf, 3, true, state.m_sp_spans[cCEM6], &state.m_sp_slam_to_line_error[cCEM6], nullptr, cem6_ortho_error_rgba);
	state.m_sp_valid[cCEM6] = true;
		
	if (num_comps == 4)
	{
		// CEM 8: RGB Direct: A=always 255
		fit_and_measure(temp_pbuf, 3, false, state.m_sp_spans[cCEM8], &state.m_sp_slam_to_line_error[cCEM8], nullptr, nullptr);
		state.m_sp_valid[cCEM8] = true;

		// CEM 10: RGB Base+Scale Plus Two A
		fit_and_measure(temp_pbuf, 4, true, state.m_sp_spans[cCEM10], &state.m_sp_slam_to_line_error[cCEM10], nullptr, cem10_ortho_error_rgba);
		state.m_sp_valid[cCEM10] = true;

		// CEM 12: RGBA Direct - also general block stats
		fit_and_measure(temp_pbuf, 4, false, state.m_sp_spans[cCEM12], &state.m_sp_slam_to_line_error[cCEM12], &state.m_stats, nullptr);
		state.m_sp_valid[cCEM12] = true;
	}
	else
	{
		// CEM 8: RGB Direct: A=always 255
		fit_and_measure(temp_pbuf, 3, false, state.m_sp_spans[cCEM8], &state.m_sp_slam_to_line_error[cCEM8], &state.m_stats, nullptr);
		state.m_sp_valid[cCEM8] = true;
	}

	// Dual Plane
	const float CEM6_10_DP_RGB_CHAN_ORTHO_WEIGHT = .25f;
	
	for (uint32_t comp = 0; comp < num_comps; comp++)
	{
		pixelbuf_swap_comp_with_alpha(temp_pbuf, comp);

		// CCS must be [0,2] for CEM 6/8 dual plane (they can't encode alpha, it's set to 255)
		if (comp < 3)
		{
			// CEM 6: Although R,G, or B are on a separate plane, they still share the same Scale factor. A=always 255.
			state.m_dp_spans[cCEM6][comp][0] = state.m_sp_spans[cCEM6][0];
			state.m_dp_spans[cCEM6][comp][1] = state.m_sp_spans[cCEM6][1];
			state.m_dp_spans[cCEM6][comp][2] = state.m_sp_spans[cCEM6][2];
			state.m_dp_slam_to_line_error[cCEM6][comp] = a_slam_to_255_sse;
			for (uint32_t c = 0; c < 3; c++)
			{
				const float chan_weight = (c == comp) ? CEM6_10_DP_RGB_CHAN_ORTHO_WEIGHT : 1.0f;
				state.m_dp_slam_to_line_error[cCEM6][comp] += cem6_ortho_error_rgba[c] * chan_weight;
			}
			state.m_dp_valid[cCEM6][comp] = true;

			// CEM 8: RGB Direct, R,G,B on a separate plane, A=always 255.
			// Source A has been rotated somewhere into RGB, so we zero that out, so it's only 2D PCA on the remaining channels on weight plane 0.
			fit_and_measure_2D(temp_pbuf, comp, false, state.m_dp_spans[cCEM8][comp], &state.m_dp_slam_to_line_error[cCEM8][comp], nullptr, nullptr);
			state.m_dp_spans[cCEM8][comp][comp] = 0; // force it to 0 in case forcing channel comp to 0 made the block entire solid, causing PCA to return the gray axis
			
			state.m_dp_spans[cCEM8][comp][3] = chan_ranges[comp];
			std::swap(state.m_dp_spans[cCEM8][comp][3], state.m_dp_spans[cCEM8][comp][comp]);
			state.m_dp_slam_to_line_error[cCEM8][comp] += a_slam_to_255_sse;
			state.m_dp_valid[cCEM8][comp] = true;
		}

		if (num_comps == 4)
		{
			// CEM 10: RGB Base+Scale + Two A, R,G,B or A on a separate plane
			if (comp == 3)
			{
				// This is just sp CEM6 + entirely separate alpha, so slam to line error is just RGB
				state.m_dp_spans[cCEM10][comp][0] = state.m_sp_spans[cCEM6][0];
				state.m_dp_spans[cCEM10][comp][1] = state.m_sp_spans[cCEM6][1];
				state.m_dp_spans[cCEM10][comp][2] = state.m_sp_spans[cCEM6][2];
				state.m_dp_spans[cCEM10][comp][3] = chan_ranges[3];
				state.m_dp_slam_to_line_error[cCEM10][comp] = state.m_sp_slam_to_line_error[cCEM6];
			}
			else
			{
				// this is sp CEM10 but one RGB channel is given its own weight plane after encoding, so slam to line error is reduced a bit on the ccs channel
				state.m_dp_spans[cCEM10][comp][0] = state.m_sp_spans[cCEM10][0];
				state.m_dp_spans[cCEM10][comp][1] = state.m_sp_spans[cCEM10][1];
				state.m_dp_spans[cCEM10][comp][2] = state.m_sp_spans[cCEM10][2];
				state.m_dp_spans[cCEM10][comp][3] = state.m_sp_spans[cCEM10][3];
				
				state.m_dp_slam_to_line_error[cCEM10][comp] = 0.0f;
				for (uint32_t c = 0; c < 4; c++)
				{
					const float chan_weight = (c == comp) ? CEM6_10_DP_RGB_CHAN_ORTHO_WEIGHT : 1.0f;
					state.m_dp_slam_to_line_error[cCEM10][comp] += cem10_ortho_error_rgba[c] * chan_weight;
				}
			}
			state.m_dp_valid[cCEM10][comp] = true;

			// CEM 12: RGBA Direct, R,G,B, or A on a separate plane
			fit_and_measure(temp_pbuf, 3, false, state.m_dp_spans[cCEM12][comp], &state.m_dp_slam_to_line_error[cCEM12][comp], nullptr, nullptr);
			state.m_dp_spans[cCEM12][comp][3] = chan_ranges[comp];
			std::swap(state.m_dp_spans[cCEM12][comp][3], state.m_dp_spans[cCEM12][comp][comp]);
			state.m_dp_valid[cCEM12][comp] = true;
		}

		pixelbuf_swap_comp_with_alpha(temp_pbuf, comp);

	} // comp

	// Add slam alpha to 255 penalties for CEM6/CEM8.
	state.m_sp_slam_to_line_error[cCEM6] += a_slam_to_255_sse;
	state.m_sp_slam_to_line_error[cCEM8] += a_slam_to_255_sse;

	assert(dct.cols() == (int)state.m_block_width);
	assert(dct.rows() == (int)state.m_block_height);

	// Apply Parseval's theorem to rapidly estimate SSE due to weight grid downsampling.
	float dct_temp[12 * 12];
		
	for (uint32_t c = 0; c < num_comps; c++)
		dct.forward(temp_buf + PIXELBUF_COMP_PITCH * c, PIXELBUF_ROW_PITCH, temp_buf + PIXELBUF_COMP_PITCH * c, PIXELBUF_ROW_PITCH, dct_temp);

	memset(state.m_downsample_sse, 0, sizeof(state.m_downsample_sse));

	for (uint32_t y = 0; y < state.m_block_height; y++)
	{
		for (uint32_t x = 0; x < state.m_block_width; x++)
		{
			float total_comp_energy = 0;
			for (uint32_t c = 0; c < num_comps; c++)
				total_comp_energy += square(pixelbuf_get_comp(temp_buf, x, y, c));
			
			pixelbuf_set_comp(temp_buf, x, y, 0, total_comp_energy);
		} // x
	} // y
	
#if	BASISU_USE_ENERGY_PREFIX_SUM
	prefix_sum_t prefix_sum_buf[PIXELBUF_COMP_PITCH]; // only 1 component is used
	const prefix_sum_t total_ac_energy = prepare_dct_energy_prefix_table(state.m_block_width, state.m_block_height, temp_buf, prefix_sum_buf);
#endif
				
	for (uint32_t grid_h = 2; grid_h <= state.m_block_height; grid_h++)
	{
		for (uint32_t grid_w = 2; grid_w <= state.m_block_width; grid_w++)
		{
			// check for valid grid size
			if ((grid_w * grid_h) > 64)
				continue;
						
#if	BASISU_USE_ENERGY_PREFIX_SUM
			const prefix_sum_t sse = compute_lost_dct_energy_prefix_sum(state.m_block_width, state.m_block_height, prefix_sum_buf, grid_w, grid_h, total_ac_energy);
			state.m_downsample_sse[grid_h - 2][grid_w - 2] = (float)sse;
#else
			float sse = 0;
			for (uint32_t y = 0; y < state.m_block_height; y++)
			{
				for (uint32_t x = 0; x < state.m_block_width; x++)
				{
					if ((y < grid_h) && (x < grid_w))
						continue;
					sse += pixelbuf_get_comp(temp_buf, x, y, 0);
				} // x
			} // y
			state.m_downsample_sse[grid_h - 2][grid_w - 2] = sse;
#endif // BASISU_USE_ENERGY_PREFIX_SUM

		} // grid_w
	} // grid_h

	//----

	assert(state.m_num_src_block_comps == num_comps);

	if (state.m_num_src_block_comps == 3)
	{
		assert(state.m_stats.m_mean[3] == 0.0f);
		assert(state.m_stats.m_covar[3] == 0.0f);
	}
	else
	{
		assert(state.m_stats.m_mean[3] < 255.0f);
	}

	state.m_should_include_dual_plane = false;
	if (!ctx.m_disable_dual_plane)
		state.m_should_include_dual_plane = should_include_dual_plane(state, num_comps == 4);
}

// ab_sum = (2.0f * float(w_levels) - 1.0f) / (3.0f * (float(w_levels) - 1.0f)), for [2,32] (entries 0,1 invalid)
static const float g_ab_sum_tab[33] = // [0,32]
{
	0.0f, 0.0f, // 0-1 invalid
	1.000000f, 0.833333f, 0.777778f, 0.750000f, // 2-32
	0.733333f, 0.722222f, 0.714286f, 0.708333f,
	0.703704f, 0.700000f, 0.696970f, 0.694444f,
	0.692308f, 0.690476f, 0.688889f, 0.687500f,
	0.686275f, 0.685185f, 0.684211f, 0.683333f,
	0.682540f, 0.681818f, 0.681159f, 0.680556f,
	0.680000f, 0.679487f, 0.679012f, 0.678571f,
	0.678161f, 0.677778f, 0.677419f
};

// 1/(levels-1)
static const float g_intervals_recip[257] = // [0,256]
{
	0.0f,      0.0f,      1.000000f, 0.500000f, 0.333333f, 0.250000f, 0.200000f, 0.166667f, // levels 0-1 invalid
	0.142857f, 0.125000f, 0.111111f, 0.100000f, 0.090909f, 0.083333f, 0.076923f, 0.071429f,
	0.066667f, 0.062500f, 0.058824f, 0.055556f, 0.052632f, 0.050000f, 0.047619f, 0.045455f,
	0.043478f, 0.041667f, 0.040000f, 0.038462f, 0.037037f, 0.035714f, 0.034483f, 0.033333f,
	0.032258f, 0.031250f, 0.030303f, 0.029412f, 0.028571f, 0.027778f, 0.027027f, 0.026316f,
	0.025641f, 0.025000f, 0.024390f, 0.023810f, 0.023256f, 0.022727f, 0.022222f, 0.021739f,
	0.021277f, 0.020833f, 0.020408f, 0.020000f, 0.019608f, 0.019231f, 0.018868f, 0.018519f,
	0.018182f, 0.017857f, 0.017544f, 0.017241f, 0.016949f, 0.016667f, 0.016393f, 0.016129f,
	0.015873f, 0.015625f, 0.015385f, 0.015152f, 0.014925f, 0.014706f, 0.014493f, 0.014286f,
	0.014085f, 0.013889f, 0.013699f, 0.013514f, 0.013333f, 0.013158f, 0.012987f, 0.012821f,
	0.012658f, 0.012500f, 0.012346f, 0.012195f, 0.012048f, 0.011905f, 0.011765f, 0.011628f,
	0.011494f, 0.011364f, 0.011236f, 0.011111f, 0.010989f, 0.010870f, 0.010753f, 0.010638f,
	0.010526f, 0.010417f, 0.010309f, 0.010204f, 0.010101f, 0.010000f, 0.009901f, 0.009804f,
	0.009709f, 0.009615f, 0.009524f, 0.009434f, 0.009346f, 0.009259f, 0.009174f, 0.009091f,
	0.009009f, 0.008929f, 0.008850f, 0.008772f, 0.008696f, 0.008621f, 0.008547f, 0.008475f,
	0.008403f, 0.008333f, 0.008264f, 0.008197f, 0.008130f, 0.008065f, 0.008000f, 0.007937f,
	0.007874f, 0.007812f, 0.007752f, 0.007692f, 0.007634f, 0.007576f, 0.007519f, 0.007463f,
	0.007407f, 0.007353f, 0.007299f, 0.007246f, 0.007194f, 0.007143f, 0.007092f, 0.007042f,
	0.006993f, 0.006944f, 0.006897f, 0.006849f, 0.006803f, 0.006757f, 0.006711f, 0.006667f,
	0.006623f, 0.006579f, 0.006536f, 0.006494f, 0.006452f, 0.006410f, 0.006369f, 0.006329f,
	0.006289f, 0.006250f, 0.006211f, 0.006173f, 0.006135f, 0.006098f, 0.006061f, 0.006024f,
	0.005988f, 0.005952f, 0.005917f, 0.005882f, 0.005848f, 0.005814f, 0.005780f, 0.005747f,
	0.005714f, 0.005682f, 0.005650f, 0.005618f, 0.005587f, 0.005556f, 0.005525f, 0.005495f,
	0.005464f, 0.005435f, 0.005405f, 0.005376f, 0.005348f, 0.005319f, 0.005291f, 0.005263f,
	0.005236f, 0.005208f, 0.005181f, 0.005155f, 0.005128f, 0.005102f, 0.005076f, 0.005051f,
	0.005025f, 0.005000f, 0.004975f, 0.004950f, 0.004926f, 0.004902f, 0.004878f, 0.004854f,
	0.004831f, 0.004808f, 0.004785f, 0.004762f, 0.004739f, 0.004717f, 0.004695f, 0.004673f,
	0.004651f, 0.004630f, 0.004608f, 0.004587f, 0.004566f, 0.004545f, 0.004525f, 0.004505f,
	0.004484f, 0.004464f, 0.004444f, 0.004425f, 0.004405f, 0.004386f, 0.004367f, 0.004348f,
	0.004329f, 0.004310f, 0.004292f, 0.004274f, 0.004255f, 0.004237f, 0.004219f, 0.004202f,
	0.004184f, 0.004167f, 0.004149f, 0.004132f, 0.004115f, 0.004098f, 0.004082f, 0.004065f,
	0.004049f, 0.004032f, 0.004016f, 0.004000f, 0.003984f, 0.003968f, 0.003953f, 0.003937f,
	0.003922f
};

// Note: This uses 1/w_levels, not 1/(w_levels - 1), for the effective weight quantization step.
// Although the representable weights are spaced by 1/(w_levels - 1), the encoder
// refits endpoints after weight selection, so low-level weight modes behave more
// like scalar clustering with w_levels reconstruction bins. This is especially
// important for 2-level weights, where the fixed-endpoint model overestimates
// the weight error term by 4x.

#if 1
static inline float analytical_quant_est_sse(uint32_t astc_cem, uint32_t e_levels, uint32_t w_levels, const float spans[4], uint32_t num_pixels)
{
	assert((e_levels >= 2) && (e_levels <= 256) && (w_levels >= 2) && (w_levels <= 32));
	assert(spans);
	assert((astc_cem == 0) || (astc_cem == 4) || (astc_cem == 6) || (astc_cem == 8) || (astc_cem == 10) || (astc_cem == 12));

	const float Dep = g_intervals_recip[e_levels]; // endpoint quant step
	//const float Dw = g_intervals_recip[w_levels]; // weight quant step
	const float Dw = g_intervals_recip[w_levels + 1]; // weight quant step, adjusted to approximately factor in endpoint LS fit (especially less pessimestic for 2-3 level configs)
	const float ab_sum = g_ab_sum_tab[w_levels];

	// sanity checks
	assert(fabs(Dep - (1.0f / (float)(e_levels - 1))) < .000125f);
	//assert(fabs(Dw - (1.0f / (float)(w_levels - 1))) < .000125f);
	assert(fabs(Dw - (1.0f / (float)(w_levels))) < .000125f);
	assert(fabs(ab_sum - (2.0f * float(w_levels) - 1.0f) / (3.0f * (float(w_levels) - 1.0f))) < .000125f);

	//const int num_chans = ((astc_cem == 6) || (astc_cem == 8)) ? 3 : 4;
	//const int num_chans = (astc_cem <= 8) ? 3 : 4;
	const int num_chans = get_num_cem_chans(astc_cem);

	// For num_chans = 3 we know the decoder always outputs 255 for Alpha, so there isn't any endpoint quant error there, and spans[3] should always be 0.
	if (num_chans == 3)
	{
		assert(spans[3] == 0.0f);
	}

	// 5418.75f = ((255.0f * 255.0f) / 12.0f)
	float pixel_sse = (e_levels == 256) ? 0.0f : ((Dep * Dep) * ab_sum * 5418.75f * (float)num_chans);

	const float k = (Dw * Dw) * (1.0f / 12.0f);

	float t = (spans[0] * spans[0] + spans[1] * spans[1] + spans[2] * spans[2]);
	if (num_chans == 4) //(astc_cem > 8)
		t += spans[3] * spans[3];

	pixel_sse += k * t;

	return pixel_sse * float(num_pixels);
}
#else
static inline float analytical_quant_est_sse(uint32_t astc_cem, uint32_t e_levels, uint32_t w_levels, const float spans[4], uint32_t num_pixels)
{
	assert((e_levels >= 2) && (e_levels <= 256) && (w_levels >= 2) && (w_levels <= 32));
	assert(spans);
	assert((astc_cem == 0) || (astc_cem == 4) || (astc_cem == 6) || (astc_cem == 8) || (astc_cem == 10) || (astc_cem == 12));

	const float Dep = 1.0f / (float)(e_levels - 1); // endpoint quant step
	//const float Dw = 1.0f / (float)(w_levels - 1); // weight quant step
	const float Dw = 1.0f / (float)(w_levels); // weight quant step, adjusted to approximately factor in endpoint LS fit (especially less pessimestic for 2-3 level configs)

	assert(fabs(g_intervals_recip[w_levels + 1] - Dw) < TINY_EPS);

	// TODO: precompute
	const float N = float(w_levels);
	const float ab_sum = (2.0f * N - 1.0f) / (3.0f * (N - 1.0f)); // simple model, assumes uniform weights, estimates how much endpoint quantization error survives after interpolation, averaged over all weight levels.

	//const int num_chans = ((astc_cem == 6) || (astc_cem == 8)) ? 3 : 4;
	const int num_chans = get_num_cem_chans(astc_cem);

	// For num_chans = 3 we know the decoder always outputs 255 for Alpha, so there isn't any endpoint quant error there, and spans[3] should always be 0.
	if (num_chans == 3)
	{
		assert(spans[3] == 0.0f);
	}

	float pixel_sse = (e_levels == 256) ? 0.0f : ((Dep * Dep) * ((1.0f / 12.0f) * ab_sum * (255.0f * 255.0f)) * (float)num_chans);

	const float k = (Dw * Dw) * (1.0f / 12.0f);
	for (int i = 0; i < num_chans; i++)
		pixel_sse += k * (float)(spans[i] * spans[i]);

	return pixel_sse * float(num_pixels);
}
#endif

const float DEF_SCALE_WEIGHT = 1.0f; // 1.0-3.0 seems reasonable
const float DEF_QUANT_WEIGHT = 1.0f;

// Does NOT include downsample SSE.
static inline float estimate_base_config_sse(single_subset_shortlist_state&state, const astc_unpacked_config& cfg)
{
	const uint32_t astc_cem = cfg.m_cem;
	//assert((astc_cem >= 6) && (astc_cem <= 12));
	assert(is_cem_0_or_4(astc_cem) || is_cem_6_or_10(astc_cem) || is_cem_8_or_12(astc_cem));

	//float error = 0.0f;

	uint32_t cem_index = 0; // = (astc_cem - 6) >> 1;
	if (astc_cem >= 6)
		cem_index = (astc_cem - 6) >> 1;
	else if (astc_cem == 0)
		cem_index = cCEM8;
	else
	{
		assert(astc_cem == 4);
		cem_index = cCEM12;
	}

	const int num_endpoint_levels = astc_helpers::get_ise_levels(cfg.m_endpoint_range);
	const int num_weight_levels = astc_helpers::get_ise_levels(cfg.m_weight_range);

	const bool dual_plane = cfg.m_dual_plane;
	const uint32_t ccs_index = cfg.m_ccs_index;
	
	// sanity check, decoded A will always be 255 here, A span should be 0
	if ((dual_plane) && (ccs_index == 3))
	{
		// forbidden combo, useless
		assert((astc_cem != 0) && (astc_cem != 6) && (astc_cem != 8));
	}

	const int num_block_pixels = state.m_block_height * state.m_block_width;

	// sanity check
	assert(dual_plane ? state.m_dp_valid[cem_index][ccs_index] : state.m_sp_valid[cem_index]);

	const float quant_error = analytical_quant_est_sse(astc_cem, num_endpoint_levels, num_weight_levels, 
		dual_plane ? state.m_dp_spans[cem_index][ccs_index] : state.m_sp_spans[cem_index], num_block_pixels);

	const float ortho_error = dual_plane ? state.m_dp_slam_to_line_error[cem_index][ccs_index] : state.m_sp_slam_to_line_error[cem_index];

	return quant_error * DEF_QUANT_WEIGHT + ortho_error;
}

// also adds in downsample SSE
static inline float estimate_full_config_sse(single_subset_shortlist_state& state, const astc_unpacked_config& cfg, float scale_weight)
{
	return state.m_downsample_sse[cfg.m_grid_height - 2][cfg.m_grid_width - 2] * scale_weight + estimate_base_config_sse(state, cfg);
}

static inline float estimate_full_config_sse(single_subset_shortlist_state& state, const astc_unpacked_config& cfg, float base_sse, float scale_weight)
{
	return state.m_downsample_sse[cfg.m_grid_height - 2][cfg.m_grid_width - 2] * scale_weight + base_sse;
}

[[maybe_unused]] static inline void unpack_config(astc_unpacked_config& cfg, uint32_t packed_config)
{
	cfg.m_cem = (uint8_t)extract_bits(packed_config, 0, 4);
	cfg.m_grid_width = (uint8_t)extract_bits(packed_config, 4, 4);
	cfg.m_grid_height = (uint8_t)extract_bits(packed_config, 8, 4);
	cfg.m_endpoint_range = (uint8_t)extract_bits(packed_config, 12, 5);
	cfg.m_weight_range = (uint8_t)extract_bits(packed_config, 17, 4);
	cfg.m_dual_plane = (uint8_t)extract_bits(packed_config, 21, 1) != 0;
	cfg.m_ccs_index = 0;
	cfg.m_unused = 0;
}

static inline void estimate_and_add_config(
	single_subset_shortlist_state& shortlist_state,
	float sse, const astc_unpacked_config&cfg,
	float &max_candidate_sse, uint32_t &num_candidates, uint32_t max_candidates)
{
	if (num_candidates < max_candidates)
	{
		assert((num_candidates >= 0) && (num_candidates < std::size(shortlist_state.m_best_configs)));

		memcpy(&shortlist_state.m_best_configs[num_candidates], &cfg, sizeof(cfg));
		shortlist_state.m_best_sse[num_candidates] = sse;

		max_candidate_sse = maximum(max_candidate_sse, sse);

		num_candidates++;
		
		return;
	}

	if (sse >= max_candidate_sse)
		return;

	assert(num_candidates == max_candidates);
		
	astc_unpacked_config worst_cfg(cfg);
	float worst_sse = sse;

	float prev_max_candidate_sse = max_candidate_sse;
	max_candidate_sse = 0;

	for (uint32_t i = 0; i < num_candidates; i++)
	{
		if (shortlist_state.m_best_sse[i] > worst_sse)
		{
			std::swap(shortlist_state.m_best_sse[i], worst_sse);
			std::swap(shortlist_state.m_best_configs[i], worst_cfg);
		}

		max_candidate_sse = basisu::maximum<float>(max_candidate_sse, shortlist_state.m_best_sse[i]);
	}

	assert(worst_sse == prev_max_candidate_sse);
	BASISU_NOTE_UNUSED(prev_max_candidate_sse);
}

static void init_single_subset_shortlist_state(
	const rgba32_image& src_block_rgba32,
	const single_subset_enc_context& context,
	single_subset_shortlist_state& shortlist_state,
	bool src_is_luma_only,
	uint32_t num_src_block_comps)
{
	shortlist_state.m_block_width = context.m_block_width;
	shortlist_state.m_block_height = context.m_block_height;
	shortlist_state.m_max_candidates = context.m_max_candidates;
	shortlist_state.m_num_src_block_comps = num_src_block_comps;
	shortlist_state.m_src_is_luma_only = src_is_luma_only;

	// TODO
	pixelbuf& pbuf = shortlist_state.m_pbuf;
	pbuf.m_width = context.m_block_width;
	pbuf.m_height = context.m_block_height;
	pbuf.m_pBuf = shortlist_state.m_block_pixels;

	pixelbuf_load_block(pbuf, src_block_rgba32, 0, 0, num_src_block_comps);
		
	compute_block_metrics(shortlist_state, num_src_block_comps, context.m_dct, context);

#if defined(DEBUG) || defined(_DEBUG)
	{
		float min_a = 255.0f;
		for (uint32_t y = 0; y < pbuf.m_height; y++)
			for (uint32_t x = 0; x < pbuf.m_width; x++)
				min_a = minimum(min_a, pixelbuf_get_comp(pbuf.m_pBuf, x, y, 3));

		if (min_a == 255.0f)
		{
			assert(num_src_block_comps == 3);
		}
		else
		{
			assert(num_src_block_comps == 4);
		}
	}
#endif
}

static uint32_t generate_single_subset_shortlist(
	uint32_t total_configs, const uint32_t *pPacked_configs,
	const single_subset_enc_context &context, 
	[[maybe_unused]] const rgba32_image &src_block_rgba32,
	[[maybe_unused]] bool src_is_luma_only,
	uint32_t num_src_block_comps, // 3 or 4, determined by caller
	single_subset_shortlist_state& shortlist_state, 
	float scale_weight, uint32_t max_candidates)
{
	static_assert(sizeof(astc_unpacked_config) == sizeof(uint64_t), "sizeof(astc_unpacked_config) != sizeof(uint64_t)");
	assert((num_src_block_comps == 3) || (num_src_block_comps == 4));
	assert(max_candidates <= MAX_CANDIDATES);

	const bool has_alpha = (num_src_block_comps == 4);
	const uint32_t num_actual_block_chans = has_alpha ? 4 : 3;
					
	uint32_t num_candidates = 0;
	float max_candidate_sse = 0;

	astc_unpacked_config cfg;
	clear_obj(cfg);
		
	for (uint32_t id = 0; id < total_configs; id++)
	{
		const uint32_t packed_config = pPacked_configs[id];

		cfg.m_cem = (uint8_t)extract_bits(packed_config, 0, 4);

		if ((num_actual_block_chans < 4) && (does_cem_have_alpha(cfg.m_cem)))
			continue;

		cfg.m_grid_width = (uint8_t)extract_bits(packed_config, 4, 4);
		cfg.m_grid_height = (uint8_t)extract_bits(packed_config, 8, 4);
		
		assert(cfg.m_grid_width >= cfg.m_grid_height);

		const bool wh_flag = (cfg.m_grid_width <= context.m_block_width) && (cfg.m_grid_height <= context.m_block_height);
		if (!wh_flag)
		{
			// hw_flag cannot be true, grid_width is >= grid_height, and block_width >= block_height, so swapping GW/GH isn't going to help.
			assert(!((cfg.m_grid_height <= context.m_block_width) && (cfg.m_grid_width <= context.m_block_height)));
			continue;
		}
		
		const bool hw_flag = (cfg.m_grid_width != cfg.m_grid_height) && (cfg.m_grid_height <= context.m_block_width) && (cfg.m_grid_width <= context.m_block_height);

		cfg.m_endpoint_range = (uint8_t)extract_bits(packed_config, 12, 5);
		cfg.m_weight_range = (uint8_t)extract_bits(packed_config, 17, 4);
		cfg.m_dual_plane = (uint8_t)extract_bits(packed_config, 21, 1) != 0;
										
#if 0
		// HACK HACK
		//if (!cfg.m_dual_plane)
		//	continue;
		if ((cfg.m_cem != 0) && (cfg.m_cem != 8) && (cfg.m_cem != 12))
			continue;
		//if (cfg.m_cem == 6)
		//	continue;
		//if (!cfg.m_dual_plane)
		//	continue;
#endif

		if (cfg.m_dual_plane)
		{
			assert(cfg.m_cem != 0);

			if (!shortlist_state.m_should_include_dual_plane)
				continue;

			uint32_t ccs_first = 0, ccs_last = num_actual_block_chans - 1;
				
			if (!does_cem_have_alpha(cfg.m_cem))
			{
				ccs_last = 2;
			}
			else if (cfg.m_cem == 4)
			{
				assert(num_actual_block_chans == 4);

				ccs_first = 3;
			}

			if (hw_flag)
			{
				astc_unpacked_config alt_cfg(cfg);
				std::swap(alt_cfg.m_grid_width, alt_cfg.m_grid_height);

				for (uint32_t ccs_index = ccs_first; ccs_index <= ccs_last; ccs_index++)
				{
					cfg.m_ccs_index = (uint8_t)ccs_index;
					
					const float base_sse = estimate_base_config_sse(shortlist_state, cfg);

					const float wh_sse = estimate_full_config_sse(shortlist_state, cfg, base_sse, scale_weight);
					estimate_and_add_config(shortlist_state, wh_sse, cfg, max_candidate_sse, num_candidates, max_candidates);

					alt_cfg.m_ccs_index = (uint8_t)ccs_index;
					const float hw_sse = estimate_full_config_sse(shortlist_state, alt_cfg, base_sse, scale_weight);
					estimate_and_add_config(shortlist_state, hw_sse, alt_cfg, max_candidate_sse, num_candidates, max_candidates);

					assert(equal_abs_tol(wh_sse, estimate_full_config_sse(shortlist_state, cfg, scale_weight), TINY_EPS));
					assert(equal_abs_tol(hw_sse, estimate_full_config_sse(shortlist_state, alt_cfg, scale_weight), TINY_EPS));
				}
			}
			else
			{
				for (uint32_t ccs_index = ccs_first; ccs_index <= ccs_last; ccs_index++)
				{
					cfg.m_ccs_index = (uint8_t)ccs_index;
					estimate_and_add_config(shortlist_state, estimate_full_config_sse(shortlist_state, cfg, scale_weight), cfg, max_candidate_sse, num_candidates, max_candidates);
				}
			}
		}
		else
		{
			cfg.m_ccs_index = 0;

			if (hw_flag)
			{
				const float base_sse = estimate_base_config_sse(shortlist_state, cfg);
				
				const float wh_sse = estimate_full_config_sse(shortlist_state, cfg, base_sse, scale_weight);
				assert(equal_abs_tol(wh_sse, estimate_full_config_sse(shortlist_state, cfg, scale_weight), TINY_EPS));
				estimate_and_add_config(shortlist_state, wh_sse, cfg, max_candidate_sse, num_candidates, max_candidates);

				std::swap(cfg.m_grid_width, cfg.m_grid_height);
				const float hw_sse = estimate_full_config_sse(shortlist_state, cfg, base_sse, scale_weight);
				assert(equal_abs_tol(hw_sse, estimate_full_config_sse(shortlist_state, cfg, scale_weight), TINY_EPS));
				estimate_and_add_config(shortlist_state, hw_sse, cfg, max_candidate_sse, num_candidates, max_candidates);
			}
			else
			{
				estimate_and_add_config(shortlist_state, estimate_full_config_sse(shortlist_state, cfg, scale_weight), cfg, max_candidate_sse, num_candidates, max_candidates);
			}
		}

	} // id

	assert((num_candidates > 0) && (num_candidates <= max_candidates));

	return num_candidates;
}

// Each subtable is the pseudoinverse of the corresponding 1D ASTC bilinear
// upsample matrix T, computed as Tplus = inverse(transpose(T) * T) * transpose(T).
// For a block size B and grid size G, the subtable contains B*G coefficients.
// The natural matrix shape is Tplus[G][B] ([row][col]) (destination major), but the downsample coefficients are stored
// transposed/source-major as filter[b * G + g] = Tplus[g][b].
// T=1D ASTC bilinear upsample matrix
// Tplus = (T^T * T) ^ -1 * T^T (T^T=T transposed)
// Input:
//   T: block_size x grid_size
// Output:
//   Tplus: grid_size x block_size
static const float s_pseudoinverse_coeffs[1585] =
{
	#include "basisu_astc_ldr_pseudoinv_tab.inl"
};

// offsets into s_pseudoinverse_coeffs[] for each possible 1D downsampling scenario
static int16_t g_pseudoinverse_coeff_offsets[9][11] = // [block_size-4][grid_size-2]
{
	// dest grid width range: 2-12												source block texel size
	{ 0,   8,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, -1 },    					// 4
	{ 20,  30,  45,  -1,  -1,  -1,  -1,  -1,  -1,  -1, -1 },   					// 5
	{ 65,  77,  95,  119,  -1,  -1,  -1,  -1,  -1,  -1, -1 },   				// 6
	{ -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, -1 },   					// 7
	{ 149,  165,  189,  221,  261,  309,  -1,  -1,  -1,  -1, -1 },     			// 8
	{ -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, -1 },  					// 9
	{ 365,  385,  415,  455,  505,  565,  635,  715,  -1,  -1, -1 },  			// 10
	{ -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, -1 },  					// 11
	{ 805,  829,  865,  913,  973,  1045,  1129,  1225,  1333,  1453, -1 }  	// 12
};

static void pseudoinverse_block_to_grid(const pixelbuf& src, pixelbuf& dst, uint32_t num_comps)
{
	assert((num_comps == 1) || (num_comps == 3) || (num_comps == 4));

	const uint32_t src_width = src.m_width, src_height = src.m_height;
	const uint32_t dst_width = dst.m_width, dst_height = dst.m_height;

	// sanity checks
	assert(&src != &dst);

	assert((src_width >= 4) && (src_width <= 12));
	assert((src_height >= 4) && (src_height <= 12));

	assert((src_width != 7) && (src_width != 9) && (src_width != 11));
	assert((src_height != 7) && (src_height != 9) && (src_height != 11));

	assert((dst_width >= 2) && (dst_width <= src_width));
	assert((dst_height >= 2) && (dst_height <= src_height));
	assert((dst_width * dst_height) <= 64); // ASTC max grid size limitation

	if ((src_width == dst_width) && (src_height == dst_height))
	{
		memcpy(dst.m_pBuf, src.m_pBuf, PIXELBUF_SIZE_IN_FLOATS * sizeof(float));
		return;
	}

	const int h_ofs = g_pseudoinverse_coeff_offsets[src_width - 4][dst_width - 2]; // across X (left/right)
	assert((h_ofs < 0) || (h_ofs + src_width * dst_width) <= std::size(s_pseudoinverse_coeffs));
	const float* pH_coeffs = (h_ofs < 0) ? nullptr : &s_pseudoinverse_coeffs[h_ofs];

	const int v_ofs = g_pseudoinverse_coeff_offsets[src_height - 4][dst_height - 2]; // across Y (up/down)
	assert((v_ofs < 0) || (v_ofs + src_height * dst_height) <= std::size(s_pseudoinverse_coeffs));
	const float* pV_coeffs = (v_ofs < 0) ? nullptr : &s_pseudoinverse_coeffs[v_ofs];

	float temp_buf[12][4]; // src_width x 1

	// TODO: compute # of ops to do vertical vs. horizontal first, use minimum
	for (uint32_t dst_y = 0; dst_y < dst_height; dst_y++)
	{
		// first filter vertically, outputs src_width x 1 samples
		if (src_height == dst_height)
		{
			assert(v_ofs == -1);

			for (uint32_t src_x = 0; src_x < src_width; src_x++)
			{
				for (uint32_t c = 0; c < num_comps; c++)
					temp_buf[src_x][c] = pixelbuf_get_comp(src.m_pBuf, src_x, dst_y, c);
			} // src_x
		}
		else
		{
			assert(v_ofs != -1);

			if (num_comps == 1)
			{
				for (uint32_t src_x = 0; src_x < src_width; src_x++)
				{
					float r = 0;

					for (uint32_t src_y = 0; src_y < src_height; src_y++)
					{
						const float w = pV_coeffs[src_y * dst_height + dst_y];

						r += pixelbuf_get_comp(src.m_pBuf, src_x, src_y, 0) * w;
					}

					temp_buf[src_x][0] = r;
				}
			}
			else
			{
				for (uint32_t src_x = 0; src_x < src_width; src_x++)
				{
					float r = 0, g = 0, b = 0, a = 0;

					for (uint32_t src_y = 0; src_y < src_height; src_y++)
					{
						const float w = pV_coeffs[src_y * dst_height + dst_y];

						r += pixelbuf_get_comp(src.m_pBuf, src_x, src_y, 0) * w;
						g += pixelbuf_get_comp(src.m_pBuf, src_x, src_y, 1) * w;
						b += pixelbuf_get_comp(src.m_pBuf, src_x, src_y, 2) * w;
						if (num_comps == 4)
							a += pixelbuf_get_comp(src.m_pBuf, src_x, src_y, 3) * w;
					}

					temp_buf[src_x][0] = r;
					temp_buf[src_x][1] = g;
					temp_buf[src_x][2] = b;
					if (num_comps == 4)
						temp_buf[src_x][3] = a;
				}
			}
		}

		// input is now src_width x 1 in temp_buf
		// filter horizontally, outputs dst_width x 1
		if (src_width == dst_width)
		{
			assert(h_ofs == -1);

			for (uint32_t dst_x = 0; dst_x < dst_width; dst_x++)
			{
				for (uint32_t c = 0; c < num_comps; c++)
					pixelbuf_set_comp(dst.m_pBuf, dst_x, dst_y, c, temp_buf[dst_x][c]);
			} // dst_x
		}
		else
		{
			assert(h_ofs != -1);

			if (num_comps == 1)
			{
				for (uint32_t dst_x = 0; dst_x < dst_width; dst_x++)
				{
					float r = 0;

					for (uint32_t src_x = 0; src_x < src_width; src_x++)
					{
						const float w = pH_coeffs[src_x * dst_width + dst_x];

						r += temp_buf[src_x][0] * w;

					} // src_x

					pixelbuf_set_comp(dst.m_pBuf, dst_x, dst_y, 0, r);

				} // dst_x
			}
			else
			{
				for (uint32_t dst_x = 0; dst_x < dst_width; dst_x++)
				{
					float r = 0, g = 0, b = 0, a = 0;

					for (uint32_t src_x = 0; src_x < src_width; src_x++)
					{
						const float w = pH_coeffs[src_x * dst_width + dst_x];

						r += temp_buf[src_x][0] * w;
						g += temp_buf[src_x][1] * w;
						b += temp_buf[src_x][2] * w;
						if (num_comps == 4)
							a += temp_buf[src_x][3] * w;

					} // src_x

					pixelbuf_set_comp(dst.m_pBuf, dst_x, dst_y, 0, r);
					pixelbuf_set_comp(dst.m_pBuf, dst_x, dst_y, 1, g);
					pixelbuf_set_comp(dst.m_pBuf, dst_x, dst_y, 2, b);
					if (num_comps == 4)
						pixelbuf_set_comp(dst.m_pBuf, dst_x, dst_y, 3, a);

				} // dst_x
			}
		}
	} // dst_y
}

// pEndpoints[8] decode order two vec4F's = LR LG LB LA, HR HG HB HA
// pCEM_vals_orig[] in rank space (not ISE)
static void cem_decode(uint32_t cem, const uint8_t pCEM_vals_quantized[8], uint32_t endpoint_range, float pEndpoints[8], float *pActual_scale)
{
	assert(astc_helpers::is_cem_ldr(cem));

	const uint8_t* pCEM_vals = pCEM_vals_quantized;

	uint8_t dequantized_cem_vals[8]; // ISE 20

	if (endpoint_range < astc_helpers::BISE_256_LEVELS)
	{
		pCEM_vals = dequantized_cem_vals;
		
		const auto& endpoint_tab = astc_helpers::g_dequant_tables.get_endpoint_tab(endpoint_range);
		
		const uint32_t num_vals = astc_helpers::get_num_cem_values(cem);
		for (uint32_t i = 0; i < num_vals; i++)
			dequantized_cem_vals[i] = (uint8_t)endpoint_tab.get_rank_to_val(pCEM_vals_quantized[i]);
	}

	int v0 = pCEM_vals[0], v1 = pCEM_vals[1], v2 = pCEM_vals[2], v3 = pCEM_vals[3];

	switch (cem)
	{
	case 0:
	case 4:
	{
		// 0 or 4
		pEndpoints[0] = (float)v0;
		pEndpoints[1] = (float)v0;
		pEndpoints[2] = (float)v0;
		pEndpoints[3] = 0xFF;

		pEndpoints[4] = (float)v1;
		pEndpoints[5] = (float)v1;
		pEndpoints[6] = (float)v1;
		pEndpoints[7] = 0xFF;

		if (cem == 4)
		{
			pEndpoints[3] = (float)v2;
			pEndpoints[7] = (float)v3;
		}

		break;
	}
	case 6:
	case 10:
	{
		// 6 or 10
		pEndpoints[0] = (float)((v0 * v3) >> 8);
		pEndpoints[1] = (float)((v1 * v3) >> 8);
		pEndpoints[2] = (float)((v2 * v3) >> 8);
		pEndpoints[3] = 0xFF;

		pEndpoints[4] = (float)v0;
		pEndpoints[5] = (float)v1;
		pEndpoints[6] = (float)v2;
		pEndpoints[7] = 0xFF;

		if (cem == 10)
		{
			pEndpoints[3] = pCEM_vals[4];
			pEndpoints[7] = pCEM_vals[5];
		}

		if (pActual_scale)
			*pActual_scale = (float)v3 * (1.0f / 256.0f);

		break;
	}
	case 8:
	case 12:
	{
		// 8 or 12
		int v4 = pCEM_vals[4], v5 = pCEM_vals[5], v6 = 255, v7 = 255;

		if (cem == 12)
		{
			v6 = pCEM_vals[6];
			v7 = pCEM_vals[7];
		}

		if ((v1 + v3 + v5) >= (v0 + v2 + v4))
		{
			pEndpoints[0] = (float)v0;
			pEndpoints[1] = (float)v2;
			pEndpoints[2] = (float)v4;
			pEndpoints[3] = (float)v6;

			pEndpoints[4] = (float)v1;
			pEndpoints[5] = (float)v3;
			pEndpoints[6] = (float)v5;
			pEndpoints[7] = (float)v7;
		}
		else
		{
			astc_helpers::blue_contract(v0, v2, v4);
			astc_helpers::blue_contract(v1, v3, v5);

			pEndpoints[0] = (float)v1;
			pEndpoints[1] = (float)v3;
			pEndpoints[2] = (float)v5;
			pEndpoints[3] = (float)v7;

			pEndpoints[4] = (float)v0;
			pEndpoints[5] = (float)v2;
			pEndpoints[6] = (float)v4;
			pEndpoints[7] = (float)v6;
		}

		if (pActual_scale)
			*pActual_scale = 0;

		break;
	}
	default:
	{
		assert(cem <= 13); // LDR check

		int dec_endpoints[4][2]; // [c][l/h]
		astc_helpers::decode_endpoint(cem, dec_endpoints, pCEM_vals);

		pEndpoints[0] = (float)dec_endpoints[0][0];
		pEndpoints[1] = (float)dec_endpoints[1][0];
		pEndpoints[2] = (float)dec_endpoints[2][0];
		pEndpoints[3] = (float)dec_endpoints[3][0];

		pEndpoints[4] = (float)dec_endpoints[0][1];
		pEndpoints[5] = (float)dec_endpoints[1][1];
		pEndpoints[6] = (float)dec_endpoints[2][1];
		pEndpoints[7] = (float)dec_endpoints[3][1];
		
		if (pActual_scale)
			*pActual_scale = 0;

		break;
	}
	}

#if defined(DEBUG) || defined(_DEBUG)
	{
		// sanity check
		int dec_endpoints[4][2]; // [c][l/h]
		astc_helpers::decode_endpoint(cem, dec_endpoints, pCEM_vals);
		for (uint32_t i = 0; i < 4; i++)
		{
			assert((float)dec_endpoints[i][0] == pEndpoints[i]);
			assert((float)dec_endpoints[i][1] == pEndpoints[4 + i]);
		}
	}
#endif
}

// values in decoded L RGBA H RGBA order (not cem encode order)
static float calc_shortest_endpoint_dist(const float a[8], const float b[8], uint32_t num_comps)
{
	assert((num_comps == 3) || (num_comps == 4));
	float dist0, dist1;

	if (num_comps == 3)
	{
		dist0 = vec3_squared_dist(a + 0, b + 0) + vec3_squared_dist(a + 4, b + 4);
		dist1 = vec3_squared_dist(a + 0, b + 4) + vec3_squared_dist(a + 4, b + 0);
	}
	else
	{
		dist0 = vec4_squared_dist(a + 0, b + 0) + vec4_squared_dist(a + 4, b + 4);
		dist1 = vec4_squared_dist(a + 0, b + 4) + vec4_squared_dist(a + 4, b + 0);
	}

	return minimum(dist0, dist1);
}

static inline int quant_endpoint_val_to_rank(float value, uint32_t range, uint32_t num_levels)
{
	(void)range;
	
	if (basisu::is_pow2(num_levels))
	{
		return clamp<int>((int)(value * (1.0f / 255.0f) * (num_levels - 1) + 0.5f), 0, num_levels - 1);
	}
	else
	{
		// TODO: Compute optimal rounding tables
		value = clamp(value, 0.0f, 255.0f);
		int v = (int)value;
		
		const auto& endpoint_tab = astc_helpers::g_dequant_tables.get_endpoint_tab(range);
		
		int r0 = endpoint_tab.get_val_to_rank(v);
		float v0 = fabs((float)endpoint_tab.get_rank_to_val(r0) - value);

		int rp = minimum<int>(r0 + 1, num_levels - 1);
		float vp = fabs((float)endpoint_tab.get_rank_to_val(rp) - value);

		return (v0 < vp) ? r0 : rp;
	}
}

// returns [0,255]
static inline int dequant_endpoint_rank_to_val(uint32_t rank, uint32_t range)
{
	return astc_helpers::g_dequant_tables.get_endpoint_tab(range).get_rank_to_val(rank);
}

// returns [0,64]
[[maybe_unused]] static inline int dequant_weight_rank_to_val(uint32_t rank, uint32_t range)
{
	assert((range <= astc_helpers::BISE_32_LEVELS) || (range == astc_helpers::BISE_64_LEVELS));

	if (range == astc_helpers::BISE_64_LEVELS)
		return rank;
	else
		return astc_helpers::g_dequant_tables.get_weight_tab(range).get_rank_to_val(rank);
}

[[maybe_unused]] static inline int apply_delta_to_rank_value(int rank_val, int delta, uint32_t num_levels)
{
	return clamp<int>(rank_val + delta, 0, num_levels - 1);
}

static inline int compute_endpoint_sum(const uint8_t pCEM_values[6], uint32_t endpoint_ise_range)
{
	const auto& endpoint_tab = astc_helpers::g_dequant_tables.get_endpoint_tab(endpoint_ise_range);

	int sum = 0;

	for (uint32_t i = 0; i < 6; i++)
	{
		int v = endpoint_tab.get_rank_to_val(pCEM_values[i]);
		
		if ((i & 1) == 0) // low endpoints subtract
			v = -v;

		sum += v;
	}

	return sum;
}

static inline void cem_bc_encode(uint32_t cem_index, uint32_t endpoint_ise_index, uint8_t pCEM_values[8], uint32_t num_levels, bool use_bc)
{
	assert((cem_index == 8) || (cem_index == 9) || (cem_index == 12) || (cem_index == 13));

	int cur_sum = compute_endpoint_sum(pCEM_values, endpoint_ise_index);
	if (cur_sum == 0)
	{
		for (uint32_t i = 0; i < 6; i++)
		{
			int dir = (i & 1) ? 1 : -1;

			int cur_r = pCEM_values[i];
			int new_r = clamp<int>(cur_r + dir, 0, num_levels - 1);
			if (new_r == cur_r)
				continue;

			pCEM_values[i] = (uint8_t)new_r;

			cur_sum = compute_endpoint_sum(pCEM_values, endpoint_ise_index);
			if (cur_sum != 0)
				break;
		}
	}

	assert(cur_sum != 0);

	bool cur_bc = cur_sum < 0;

	if (cur_bc != use_bc)
	{
		const uint32_t num_comps = (cem_index >= 12) ? 4 : 3;

		for (uint32_t i = 0; i < num_comps; i++)
			std::swap(pCEM_values[2 * i + 0], pCEM_values[2 * i + 1]);
	}

#if defined(DEBUG) || defined(_DEBUG)
	{
		uint8_t cem_ise_vals[8];
		for (uint32_t i = 0; i < astc_helpers::get_num_cem_values(cem_index); i++)
			cem_ise_vals[i] = astc_helpers::g_dequant_tables.get_endpoint_tab(endpoint_ise_index).m_rank_to_ISE[pCEM_values[i]];

		const bool check_bc = astc_helpers::used_blue_contraction(cem_index, cem_ise_vals, endpoint_ise_index);
		// should never happen, but if it does, it's not fatal (hurts quality especially in single shot mode)
		assert(check_bc == use_bc);
	}
#endif
}

// pEndpoints[] = ASTC direct order: LR HR LG HG LB HB LA HA
// true if BC was used
static bool encode_cem_8_12(uint32_t cem_index, uint8_t* pCEM_values, uint32_t endpoint_ise_range, const float pEndpoints[8], bool allow_bc)
{
	const uint32_t num_endpoint_levels = astc_helpers::get_ise_levels(endpoint_ise_range);
	const uint32_t num_endpoint_vals = (cem_index >= 12) ? 8 : 6;

	// don't bother with BC if 256 levels, pointless
	bool use_bc = allow_bc && (num_endpoint_levels < 256);
	float enc_endpoints[8];
		
	if (use_bc)
	{
		for (uint32_t i = 0; i < 2; i++)
		{
			float r = pEndpoints[0 + i], g = pEndpoints[2 + i], b = pEndpoints[4 + i];

			r = r * 2 - b;
			g = g * 2 - b;

			float clamped_r = clamp<float>(r, 0, 255);
			float clamped_g = clamp<float>(g, 0, 255);

			if ((r != clamped_r) || (g != clamped_g))
			{
				use_bc = false;
				break;
			}

			enc_endpoints[0 + i] = clamped_r;
			enc_endpoints[2 + i] = clamped_g;
			enc_endpoints[4 + i] = b;
		}
	}

	if (!use_bc)
		vec_copy(enc_endpoints, pEndpoints, 6);

	if (cem_index == 12)
	{
		enc_endpoints[6] = pEndpoints[6];
		enc_endpoints[7] = pEndpoints[7];
	}

	for (uint32_t i = 0; i < num_endpoint_vals; i++)
		pCEM_values[i] = (uint8_t)quant_endpoint_val_to_rank(enc_endpoints[i], endpoint_ise_range, num_endpoint_levels);

	cem_bc_encode(cem_index, endpoint_ise_range, pCEM_values, num_endpoint_levels, use_bc);
	
	return use_bc;
}

// pEndpoints[] = ASTC direct order: LR HR LG HG LB HB LA HA
// scale derived from L endpoint
static void encode_cem_6_10(uint32_t cem_index, uint8_t* pCEM_values, uint32_t endpoint_ise_range, const float pEndpoints[8])
{
	assert((cem_index == 6) || (cem_index == 10));

	const uint32_t num_endpoint_levels = astc_helpers::get_ise_levels(endpoint_ise_range);
		
	float h[3];

	for (uint32_t c = 0; c < 3; c++)
	{
		pCEM_values[c] = (uint8_t)quant_endpoint_val_to_rank(pEndpoints[c * 2 + 1], endpoint_ise_range, num_endpoint_levels);

		h[c] = (float)dequant_endpoint_rank_to_val(pCEM_values[c], endpoint_ise_range);
	}

	float l[3] = { pEndpoints[0], pEndpoints[2], pEndpoints[4] };

	float hh_dot = vec3_dot(h, h);
	float lh_dot = vec3_dot(l, h);
	
	float scale = (256.0f * lh_dot) / (hh_dot + TINY_EPS);

	pCEM_values[3] = (uint8_t)quant_endpoint_val_to_rank(scale, endpoint_ise_range, num_endpoint_levels);
	
	if (cem_index == 10)
	{
		pCEM_values[4] = (uint8_t)quant_endpoint_val_to_rank(pEndpoints[6], endpoint_ise_range, num_endpoint_levels);
		pCEM_values[5] = (uint8_t)quant_endpoint_val_to_rank(pEndpoints[7], endpoint_ise_range, num_endpoint_levels);
	}
}

// pEndpoints[] = ASTC direct order: LR HR LG HG LB HB LA HA
static void encode_cem_0_4(uint32_t cem_index, uint8_t* pCEM_values, uint32_t endpoint_ise_range, const float pEndpoints[8])
{
	assert((cem_index == 0) || (cem_index == 4));

	const uint32_t num_endpoint_levels = astc_helpers::get_ise_levels(endpoint_ise_range);
			
	pCEM_values[0] = (uint8_t)quant_endpoint_val_to_rank(pEndpoints[0], endpoint_ise_range, num_endpoint_levels);
	pCEM_values[1] = (uint8_t)quant_endpoint_val_to_rank(pEndpoints[1], endpoint_ise_range, num_endpoint_levels);

	if (cem_index == 4)
	{
		pCEM_values[2] = (uint8_t)quant_endpoint_val_to_rank(pEndpoints[6], endpoint_ise_range, num_endpoint_levels);
		pCEM_values[3] = (uint8_t)quant_endpoint_val_to_rank(pEndpoints[7], endpoint_ise_range, num_endpoint_levels);
	}
}

// pEndpoints[] = ASTC direct order: LR HR LG HG LB HB LA HA
// returns false if base+ofs encode clamped
static bool encode_cem_9_13(uint32_t cem_index, uint8_t* pCEM_values, uint32_t endpoint_ise_range, const float pEndpoints[8], bool allow_bc)
{
	assert(is_cem_9_or_13(cem_index));

	const uint32_t num_chans = get_num_cem_chans(cem_index);

	basist::color_rgba e[2];
	
	e[0].a = 255;
	e[1].a = 255;

	for (uint32_t c = 0; c < num_chans; c++)
	{
		e[0][c] = (uint8_t)clamp<int>(fast_roundf_int(pEndpoints[c * 2 + 0]), 0, 255);
		e[1][c] = (uint8_t)clamp<int>(fast_roundf_int(pEndpoints[c * 2 + 1]), 0, 255);
	} // c

	bool bc_clamped_flag, base_ofs_clamped_flag, endpoints_swapped_flag;
			
	bool status = basist::astc_ldr_t::pack_base_offset(cem_index, endpoint_ise_range, pCEM_values, e[0], e[1], allow_bc, true, bc_clamped_flag, base_ofs_clamped_flag, endpoints_swapped_flag);
	BASISU_NOTE_UNUSED(status);
	assert(status);
	
	return !base_ofs_clamped_flag;
}

// source pEndpoints[] = ASTC direct order: LR HR LG HG LB HB LA HA
// output are quantized ASTC CEM values (rank space)
// true is CEM specific, for 9/13 it means the base+ofs pack didn't clamp
bool cem_encode(uint32_t cem_index, const float pEndpoints[8], uint32_t endpoint_ise_range, uint8_t* pCEM_values, bool allow_bc, bool high_effort)
{
	bool status = true;

	switch (cem_index)
	{
	case 0:
	case 4:
	{
		encode_cem_0_4(cem_index, pCEM_values, endpoint_ise_range, pEndpoints);
		break;
	}
	case 6:
	case 10:
	{
		encode_cem_6_10(cem_index, pCEM_values, endpoint_ise_range, pEndpoints);
		break;
	}
	case 8:
	case 12:
	{
		const bool used_bc = encode_cem_8_12(cem_index, pCEM_values, endpoint_ise_range, pEndpoints, allow_bc);

		if ((high_effort) && (used_bc))
		{
			// not necessary - small to tiny gain
			float ep_bc[8];
			cem_decode(cem_index, pCEM_values, endpoint_ise_range, ep_bc, nullptr);

			uint8_t CEM_values_plain[8];

			const bool used_bc2 = encode_cem_8_12(cem_index, CEM_values_plain, endpoint_ise_range, pEndpoints, false);
			BASISU_NOTE_UNUSED(used_bc2);
			assert(!used_bc2);

			float ep_plain[8];
			cem_decode(cem_index, CEM_values_plain, endpoint_ise_range, ep_plain, nullptr);

			const uint32_t num_comps = get_num_cem_chans(cem_index);

			float desired_endpoints[8];
			
			for (uint32_t c = 0; c < num_comps; c++)
			{
				desired_endpoints[c + 0] = pEndpoints[c * 2 + 0];
				desired_endpoints[c + 4] = pEndpoints[c * 2 + 1];
			}

			const float dist_bc = calc_shortest_endpoint_dist(desired_endpoints, ep_bc, num_comps);
			const float dist_plain = calc_shortest_endpoint_dist(desired_endpoints, ep_plain, num_comps);

			if (dist_plain < dist_bc)
			{
				memcpy(pCEM_values, CEM_values_plain, astc_helpers::get_num_cem_values(cem_index));
			}
		}

		break;
	}
	case 9:
	case 13:
	{
		status = encode_cem_9_13(cem_index, pCEM_values, endpoint_ise_range, pEndpoints, allow_bc);
		break;
	}
	default:
	{
		assert(0);
		memset(pCEM_values, 0, astc_helpers::get_num_cem_values(cem_index));
		status = false;
		break;
	}
	}

	return status;
}

static void eval_weights_first_plane(
	const pixelbuf& pixels, 
	uint8_t* pWeights, uint32_t weight_ise_range, 
	uint32_t cem, const uint8_t *pCEM_values, uint32_t endpoint_ise_range,
	uint32_t num_comps,
	int chan_to_swap = -1)
{
	assert((num_comps == 3) || (num_comps == 4));
	
	float dec_endpoints[8];
	cem_decode(cem, pCEM_values, endpoint_ise_range, dec_endpoints, nullptr);

	if (chan_to_swap >= 0)
	{
		assert(chan_to_swap <= 3);
		std::swap(dec_endpoints[0 + chan_to_swap], dec_endpoints[0 + 3]);
		std::swap(dec_endpoints[4 + chan_to_swap], dec_endpoints[4 + 3]);
	}

	float dir[4] = {
		dec_endpoints[4] - dec_endpoints[0],
		dec_endpoints[5] - dec_endpoints[1],
		dec_endpoints[6] - dec_endpoints[2],
		(num_comps == 4)  ? (dec_endpoints[7] - dec_endpoints[3]) : 0.0f };

	uint32_t num_weight_levels = astc_helpers::get_ise_levels(weight_ise_range);
	if (num_weight_levels == 64)
		num_weight_levels = 65; // [0,64] (special case for raw ASTC weight mode, used for intermediate calcs)
	
	const uint32_t num_weight_levels_minus_one = num_weight_levels - 1;
	
	float dir_len2 = vec4_dot(dir, dir);
	
	if (dir_len2 < TINY_EPS)
	{
		memset(pWeights, 0, pixels.m_width * pixels.m_height);
		return;
	}

	vec_scale(dir, (float)num_weight_levels_minus_one / dir_len2, num_comps);

	const float w_bias = 0.5f - vec4_dot(dec_endpoints, dir); // 0.5f=rounding
					
	uint8_t* pDst_weights = pWeights;

	if (num_comps == 4)
	{
		for (uint32_t y = 0; y < pixels.m_height; y++)
		{
			for (uint32_t x = 0; x < pixels.m_width; x++)
			{
				float v[4];
				pixelbuf_get_pixel4(pixels.m_pBuf, x, y, v);

				float w = (v[0] * dir[0]) + (v[1] * dir[1]) + (v[2] * dir[2]) + (v[3] * dir[3]) + w_bias;

				int qw = (int)(w);

				if ((uint32_t)qw > num_weight_levels_minus_one)
					qw = ((~qw) >> 31) & num_weight_levels_minus_one;

				*pDst_weights++ = (uint8_t)qw;

			} // x
		} // y
	}
	else
	{
		for (uint32_t y = 0; y < pixels.m_height; y++)
		{
			for (uint32_t x = 0; x < pixels.m_width; x++)
			{
				float v[3];
				pixelbuf_get_pixel3(pixels.m_pBuf, x, y, v);

				float w = (v[0] * dir[0]) + (v[1] * dir[1]) + (v[2] * dir[2]) + w_bias;

				int qw = (int)(w);

				if ((uint32_t)qw > num_weight_levels_minus_one)
					qw = ((~qw) >> 31) & num_weight_levels_minus_one;

				*pDst_weights++ = (uint8_t)qw;

			} // x
		} // y
	}
}

static void eval_weights_for_plane(
	const pixelbuf& pixels,
	uint32_t weight_stride, // 1 or 2
	uint8_t* pWeights, uint32_t weight_ise_range,
	uint32_t cem, const uint8_t* pCEM_values, uint32_t endpoint_ise_range,
	uint32_t active_chans_mask, 
	int block_chan_to_swap_with_alpha) // only impacts reads from pixels
{
	float dec_endpoints[8];
	cem_decode(cem, pCEM_values, endpoint_ise_range, dec_endpoints, nullptr);

	float dir[4];
	vec4_sub(dir, dec_endpoints + 4, dec_endpoints);

	const uint32_t num_comps = 4;
		
	for (uint32_t i = 0; i < 4; i++)
		if ((active_chans_mask & (1 << i)) == 0)
			dir[i] = 0;

	uint32_t num_weight_levels = astc_helpers::get_ise_levels(weight_ise_range);
	if (num_weight_levels == 64)
		num_weight_levels = 65; // [0,64] (special case for raw ASTC weight mode, used for intermediate calcs)

	const uint32_t num_weight_levels_minus_one = num_weight_levels - 1;

	float dir_len2 = vec4_dot(dir, dir);

	if (dir_len2 < TINY_EPS)
	{
		const uint32_t total_pixels = pixels.m_width * pixels.m_height;
		for (uint32_t i = 0; i < total_pixels; i++)
			pWeights[i * weight_stride] = 0;
		return;
	}

	vec_scale(dir, (float)num_weight_levels_minus_one / dir_len2, num_comps);

	const float w_bias = 0.5f - vec4_dot(dec_endpoints, dir); // 0.5f=rounding

	uint8_t* pDst_weights = pWeights;

	for (uint32_t y = 0; y < pixels.m_height; y++)
	{
		for (uint32_t x = 0; x < pixels.m_width; x++)
		{
			float v[4];
			pixelbuf_get_pixel4(pixels.m_pBuf, x, y, v);

			if (block_chan_to_swap_with_alpha >= 0)
				std::swap(v[3], v[block_chan_to_swap_with_alpha]);

			float w = (v[0] * dir[0]) + (v[1] * dir[1]) + (v[2] * dir[2]) + (v[3] * dir[3]) + w_bias;

			int qw = (int)(w);

			if ((uint32_t)qw > num_weight_levels_minus_one)
				qw = ((~qw) >> 31) & num_weight_levels_minus_one;

			*pDst_weights = (uint8_t)qw;
			pDst_weights += weight_stride;

		} // x
	} // y
}

[[maybe_unused]] static bool refine_endpoints_given_weights_cem_6_or_10_method2(
	const pixelbuf& pixels, uint32_t cem,
	const uint8_t *pWeights, uint32_t weight_ise_range,
	const uint8_t* pCEM_values, uint32_t endpoint_ise_range,
	float pNew_CEM_vals[6], // always writes 6 values
	uint32_t num_comps)
{
	assert(is_cem_6_or_10(cem));
	assert((num_comps == 3) || (num_comps == 4));

	float dec_endpoints[8], actual_scale = 0.0f;
	cem_decode(cem, pCEM_values, endpoint_ise_range, dec_endpoints, &actual_scale);

	float actual_high[3] = { dec_endpoints[4], dec_endpoints[5], dec_endpoints[6] };

	// not valid if weight_ise_range is 64 levels (which is a special case for [0,64] or 65 actual levels)
	const auto& weight_tab = astc_helpers::g_dequant_tables.get_weight_tab(minimum<uint32_t>(astc_helpers::BISE_32_LEVELS, weight_ise_range));

	float Pa[3], Pb[3];
	vec3_zero(Pa);
	vec3_zero(Pb);
	
	float A = 0.0f, B = 0.0f, C = 0.0f;

	const uint8_t* pSrc_weights = pWeights;
	for (uint32_t y = 0; y < pixels.m_height; y++)
	{
		for (uint32_t x = 0; x < pixels.m_width; x++)
		{
			float px[3];
			pixelbuf_get_pixel3(pixels.m_pBuf, x, y, px);

			const int qw = *pSrc_weights++;
			const int iw = (weight_ise_range == astc_helpers::BISE_64_LEVELS) ? qw : weight_tab.get_rank_to_val(qw);
			assert(iw <= 64);

			float t = (float)iw * (1.0f / 64.0f);
			float bi = t, ai = 1.0f - t;

			vec3_scale_add(Pa, px, ai, Pa);
			vec3_scale_add(Pb, px, bi, Pb);
			
			A += ai * ai;
			B += ai * bi;
			C += bi * bi;
		} // x
	} // y

	const float MAX_S = 255.0f / 256.0f;

	bool did_clamp = false;

	float new_high[3];
	vec3_copy(new_high, actual_high);
	float new_scale = actual_scale;

	float h2 = vec3_dot(actual_high, actual_high);
	if ((h2 > TINY_EPS) && (A > TINY_EPS))
	{
		new_scale = (vec3_dot(Pa, actual_high) / h2 - B) / A;
		new_scale = clamp(new_scale, 0.0f, MAX_S); // not setting did_clamp on intermediate new_scale
	}

	const float den = A * new_scale * new_scale + 2.0f * B * new_scale + C;
	if (den > TINY_EPS)
	{
		vec3_scale_add(new_high, Pa, new_scale, Pb);
		vec3_div(new_high, den);
		for (uint32_t i = 0; i < 3; i++)
			new_high[i] = clamp_flag(new_high[i], 0.0f, 255.0f, did_clamp);
	}

	h2 = vec3_dot(new_high, new_high);
	if ((h2 > TINY_EPS) && (A > TINY_EPS))
	{
		new_scale = (vec3_dot(Pa, new_high) / h2 - B) / A;
		new_scale = clamp_flag(new_scale, 0.0f, MAX_S, did_clamp);
	}

	for (uint32_t c = 0; c < 3; c++)
	{
		pNew_CEM_vals[c * 2 + 0] = new_scale * new_high[c];
		pNew_CEM_vals[c * 2 + 1] = new_high[c];
	}

	pNew_CEM_vals[6] = 255.0f;
	pNew_CEM_vals[7] = 255.0f;

	if (num_comps == 4)
	{
		float z00 = 0, z01 = 0, z10 = 0, z11 = 0;
		float q00_a = 0, q10_a = 0, t_a = 0;

		pSrc_weights = pWeights;
		for (uint32_t y = 0; y < pixels.m_height; y++)
		{
			for (uint32_t x = 0; x < pixels.m_width; x++)
			{
				const float a = pixelbuf_get_comp(pixels.m_pBuf, x, y, 3);

				const uint32_t qw = *pSrc_weights++;
				const int dw = (weight_ise_range <= astc_helpers::BISE_32_LEVELS) ? weight_tab.get_rank_to_val(qw) : qw;
				const float w = dw * (1.0f / 64.0f);

				z00 += w * w;
				z10 += (1.0f - w) * w;
				z11 += (1.0f - w) * (1.0f - w);

				q00_a += w * a;
				t_a += a;
			} // x
		} // y

		q10_a = t_a - q00_a;
		z01 = z10;
						
		float xl, xh;
		
		float det = z00 * z11 - z01 * z10;
		//if (fabs(det) >= TINY_EPS)
		if (det >= TINY_EPS)
		{
			det = 1.0f / det;

			float iz00 = z11 * det;
			float iz01 = -z01 * det;
			float iz10 = -z10 * det;
			float iz11 = z00 * det;
						
			xl = clamp_flag(iz10 * q00_a + iz11 * q10_a, 0.0f, 255.0f, did_clamp);
			xh = clamp_flag(iz00 * q00_a + iz01 * q10_a, 0.0f, 255.0f, did_clamp);
		}
		else
		{
			xl = t_a / (float)(pixels.m_width * pixels.m_height);
			xh = xl;
		}

		pNew_CEM_vals[6] = xl;
		pNew_CEM_vals[7] = xh;
	}

	return did_clamp;
}

static bool refine_endpoints_given_weights_cem_6_or_10_method3(
	const pixelbuf& pixels,
	uint32_t cem,
	const uint8_t* pWeights, uint32_t weight_ise_range,
	float pNew_CEM_vals[6], // always writes 6 values
	uint32_t num_comps)
{
	BASISU_NOTE_UNUSED(cem);
	assert(is_cem_6_or_10(cem));

	assert((num_comps == 3) || (num_comps == 4));
	const uint32_t pixel_count = pixels.m_width * pixels.m_height;
	const float pixel_count_f = (float)pixel_count;

	bool did_clamp = false;

	const auto& weight_tab = astc_helpers::g_dequant_tables.get_weight_tab(minimum<uint32_t>(astc_helpers::BISE_32_LEVELS, weight_ise_range));

	float sum_w = 0.0f, sum_w2 = 0.0f;
	float rgb_sum[3] = { };
	float weighted_rgb_sum[3] = { };
	
	int min_dw = 256, max_dw = 0;

	const uint8_t* pSrc_weights = pWeights;
	for (uint32_t y = 0; y < pixels.m_height; y++)
	{
		for (uint32_t x = 0; x < pixels.m_width; x++)
		{
			float p[3];
			pixelbuf_get_pixel(pixels.m_pBuf, x, y, p, 3);

			const uint32_t qw = *pSrc_weights++;
			
			const int dw = (weight_ise_range <= astc_helpers::BISE_32_LEVELS) ? weight_tab.get_rank_to_val(qw) : qw;
			assert(dw <= 64);
			
			min_dw = minimum(min_dw, dw);
			max_dw = maximum(max_dw, dw);

			const float w = dw * (1.0f / 64.0f);

			sum_w += w;
			sum_w2 += w * w;

			vec3_add(rgb_sum, p);
			vec3_scale_add(weighted_rgb_sum, p, w, weighted_rgb_sum);

		} // x
	} // y

	float rgb_mean[3];
	vec3_div(rgb_mean, rgb_sum, pixel_count_f);

	const float A = pixel_count_f - 2.0f * sum_w + sum_w2;
	const float B = sum_w - sum_w2;
	const float C = sum_w2;

	float Pb[3];
	vec3_copy(Pb, weighted_rgb_sum);

	float Pa[3]; 
	vec3_sub(Pa, rgb_sum, weighted_rgb_sum);
	
	const float kMaxScale01 = 255.0f / 256.0f;

	float best_scale01 = kMaxScale01;
	float best_obj = -1.0f; // objective is always >= 0 when valid

	auto try_scale = [&](float s)
	{
		// Reject out-of-range scales. Do not clamp roots into endpoint candidates.
		if ((s < 0.0f) || (s > kMaxScale01))
			return;

		const float den = A * s * s + 2.0f * B * s + C;
		if (den < TINY_EPS)
			return;

		float v[3];
		vec3_scale_add(v, Pa, s, Pb);

		const float obj = vec3_dot(v, v) / den;

		if (obj > best_obj)
		{
			best_obj = obj;
			best_scale01 = s;
		}
	};

	float out_high_rgb[3];
	vec3_copy(out_high_rgb, rgb_mean);
	float out_scale = kMaxScale01;

	if (max_dw == min_dw)
	{
		// weights all equal
		const float w = (float)min_dw * (1.0f / 64.0f);

		const float scale01 = kMaxScale01;

		// Decoder effective multiplier:
		// recon = lerp(scale01 * high, high, w)
		//       = high * (scale01 * (1 - w) + w)
		//       = high * (scale01 + w * (1 - scale01))
		const float f = scale01 + w * (1.0f - scale01);

		const float max_mean = fmaxf(rgb_mean[0], fmaxf(rgb_mean[1], rgb_mean[2]));

		if ((f > TINY_EPS) && (max_mean <= 255.0f * f))
		{
			// Exact constant-color reconstruction without high endpoint clamp.
			vec3_div(out_high_rgb, f);
		}
		// else: keep out_high_rgb = rgb_mean to avoid high endpoint clamp.

		vec3_clamp_flag(out_high_rgb, 0.0f, 255.0f, did_clamp);

		out_scale = scale01;
	}
	else
	{
		const float n2 = vec3_dot(Pa, Pa);
		const float n1 = vec3_dot(Pa, Pb);
		const float n0 = vec3_dot(Pb, Pb);
				
		const float q1 = n2 * C - n0 * A;
		const float q0 = n1 * C - n0 * B;
		const float q2 = n2 * B - n1 * A;

#if 0
		if ((q2 > TINY_EPS) || (q2 < -TINY_EPS))
		{
			//const double disc = (double)q1 * (double)q1 - 4.0f * (double)q2 * (double)q0;
			const float disc = q1 * q1 - 4.0f * q2 * q0;

			if (disc >= 0.0f)
			{
				//const float sqrt_disc = (float)sqrt(disc);
				const float sqrt_disc = sqrtf(disc);

				const float inv_2q2 = 0.5f / q2;

				try_scale((-q1 - sqrt_disc) * inv_2q2);
				try_scale((-q1 + sqrt_disc) * inv_2q2);
			}
		}
		else if ((q1 > TINY_EPS) || (q1 < -TINY_EPS))
		{
			try_scale(-q0 / q1);
		}
#else
		const float aq0 = fabsf(q0);
		const float aq1 = fabsf(q1);
		const float aq2 = fabsf(q2);

		const float m = fmaxf(aq0, fmaxf(aq1, aq2));

		if (m > 0.0f)
		{
			const float s = 1.0f / m;

			const float a = q2 * s;
			const float b = q1 * s;
			const float c = q0 * s;

			if (fabsf(a) > TINY_EPS)
			{
				const float disc = b * b - 4.0f * a * c;

				if (disc >= 0.0f)
				{
					const float sqrt_disc = sqrtf(disc);

					const float t = -0.5f * (b + copysignf(sqrt_disc, b));

					if (t != 0.0f)
					{
						try_scale(t / a);
						try_scale(c / t);
					}
					else
					{
						try_scale(-b * (0.5f / a));
					}
				}
			}
			else if (fabsf(b) > TINY_EPS)
			{
				try_scale(-c / b);
			}
		}
#endif

		// If no valid stationary root exists, or weights are flat,
		// use conservative fallback scales.
		if (best_obj < 0.0f)
		{
			try_scale(kMaxScale01);
			try_scale(0.875f);
			try_scale(0.75f);
			try_scale(0.5f);
			try_scale(0.25f);
		}
				
		if (best_obj >= 0.0f)
		{
			const float scale01 = best_scale01;

			const float high_den =
				A * scale01 * scale01 +
				2.0f * B * scale01 +
				C;

			if (high_den > TINY_EPS)
			{
				vec3_scale_add(out_high_rgb, Pa, scale01, Pb);
				vec3_div(out_high_rgb, high_den);
			}

			vec3_clamp_flag(out_high_rgb, 0.0f, 255.0f, did_clamp);

			out_scale = clamp_flag(scale01, 0.0f, kMaxScale01, did_clamp);
		}

	} // if (max_dw == min_dw)
	
	for (uint32_t c = 0; c < 3; c++)
	{
		pNew_CEM_vals[c * 2 + 0] = out_scale * out_high_rgb[c];
		pNew_CEM_vals[c * 2 + 1] = out_high_rgb[c];
	}

	pNew_CEM_vals[6] = 255.0f;
	pNew_CEM_vals[7] = 255.0f;

	if (num_comps == 4)
	{
		float z00 = 0, z01 = 0, z10 = 0, z11 = 0;
		float q00_a = 0, q10_a = 0, t_a = 0;

#if 0
		// TODO
		z00 = sum_w2;
		z10 = B;
		z11 = A;
		...
		q00_a = sum w * a
		t_a = sum a
		q10_a = t_a - q00_a
		...
		const float z00 = C;
		const float z01 = B;
		const float z10 = B;
		const float z11 = A;
#endif

		pSrc_weights = pWeights;
		for (uint32_t y = 0; y < pixels.m_height; y++)
		{
			for (uint32_t x = 0; x < pixels.m_width; x++)
			{
				const float a = pixelbuf_get_comp(pixels.m_pBuf, x, y, 3);

				const uint32_t qw = *pSrc_weights++;
				const int dw = (weight_ise_range <= astc_helpers::BISE_32_LEVELS) ? weight_tab.get_rank_to_val(qw) : qw;
				const float w = dw * (1.0f / 64.0f);

				// TODO: some of these values we've already computed above
				z00 += w * w;
				z10 += (1.0f - w) * w;
				z11 += (1.0f - w) * (1.0f - w);

				q00_a += w * a;
				t_a += a;
			} // x
		} // y

		q10_a = t_a - q00_a;
		z01 = z10;

		float xl, xh;

		float det = z00 * z11 - z01 * z10;
		//if (fabs(det) >= TINY_EPS) // todo should always be pos?
		if (det >= TINY_EPS) // todo should always be pos?
		{
			det = 1.0f / det;

			float iz00 = z11 * det;
			float iz01 = -z01 * det;
			float iz10 = -z10 * det;
			float iz11 = z00 * det;

			xl = clamp_flag(iz10 * q00_a + iz11 * q10_a, 0.0f, 255.0f, did_clamp);
			xh = clamp_flag(iz00 * q00_a + iz01 * q10_a, 0.0f, 255.0f, did_clamp);
		}
		else
		{
			xl = t_a / (float)(pixels.m_width * pixels.m_height);
			xh = xl;
		}

		pNew_CEM_vals[6] = xl;
		pNew_CEM_vals[7] = xh;
	}
	
	return did_clamp;
}

static bool refine_endpoints_given_weights_cem_8_9_12_or_13(
	const pixelbuf& pixels,
	uint32_t cem,
	const uint8_t* pWeights, uint32_t weight_ise_range,
	float pNew_CEM_vals[8], // always writes 8 values
	uint32_t num_comps)
{
	BASISU_NOTE_UNUSED(cem);
	assert(is_cem_8_or_12(cem) || is_cem_9_or_13(cem));
	assert((num_comps == 3) || (num_comps == 4));

	float z00 = 0, z01 = 0, z10 = 0, z11 = 0;
	float q00_a[4] = { }, t_a[4] = { };

	bool did_clamp = false;

	const auto& weight_tab = astc_helpers::g_dequant_tables.get_weight_tab(minimum<uint32_t>(astc_helpers::BISE_32_LEVELS, weight_ise_range));

	const uint8_t *pSrc_weights = pWeights;
	for (uint32_t y = 0; y < pixels.m_height; y++)
	{
		for (uint32_t x = 0; x < pixels.m_width; x++)
		{
			const uint32_t qw = *pSrc_weights++;
			const int dw = (weight_ise_range <= astc_helpers::BISE_32_LEVELS) ? weight_tab.get_rank_to_val(qw) : qw;
			assert(dw <= 64);

			const float w = dw * (1.0f / 64.0f);

			z00 += w * w;
			z10 += (1.0f - w) * w;
			z11 += (1.0f - w) * (1.0f - w);

			for (uint32_t c = 0; c < num_comps; c++)
			{
				const float a = pixelbuf_get_comp(pixels.m_pBuf, x, y, c);

				q00_a[c] += w * a;
				t_a[c] += a;
			}
		} // x
	} // y

	float q10_a[4];
	for (uint32_t c = 0; c < num_comps; c++)
		q10_a[c] = t_a[c] - q00_a[c];

	z01 = z10;
			
	float det = z00 * z11 - z01 * z10; // should be non-negative mathematically
	
	//if (fabs(det) >= TINY_EPS)
	if (det >= TINY_EPS)
	{
		det = 1.0f / det;

		float iz00 = z11 * det;
		float iz01 = -z01 * det;
		float iz10 = -z10 * det;
		float iz11 = z00 * det;

		for (uint32_t c = 0; c < num_comps; c++)
		{
			pNew_CEM_vals[c * 2 + 0] = clamp_flag(iz10 * q00_a[c] + iz11 * q10_a[c], 0.0f, 255.0f, did_clamp);
			pNew_CEM_vals[c * 2 + 1] = clamp_flag(iz00 * q00_a[c] + iz01 * q10_a[c], 0.0f, 255.0f, did_clamp);
		}
	}
	else
	{
		const float one_over_total_pixels = 1.0f / (float)(pixels.m_width * pixels.m_height);
		for (uint32_t c = 0; c < num_comps; c++)
		{
			pNew_CEM_vals[c * 2 + 0] = t_a[c] * one_over_total_pixels;
			pNew_CEM_vals[c * 2 + 1] = pNew_CEM_vals[c * 2 + 0];
		}
	}

	if (num_comps == 3)
	{
		pNew_CEM_vals[3 * 2 + 0] = 255.0f;
		pNew_CEM_vals[3 * 2 + 1] = 255.0f;
	}

	return did_clamp;
}

static bool refine_endpoints_given_weights_cem_0_or_4(
	const pixelbuf& pixels,
	uint32_t cem,
	const uint8_t* pWeights, uint32_t weight_ise_range,
	float pNew_CEM_vals[8], // always writes 8 values
	uint32_t num_comps)
{
	BASISU_NOTE_UNUSED(cem);
	BASISU_NOTE_UNUSED(num_comps);
	assert(is_cem_0_or_4(cem));
	assert((num_comps == 3) || (num_comps == 4));

	float z00 = 0, z01 = 0, z10 = 0, z11 = 0;
	float q00_a[2] = { }, t_a[2] = { };

	bool did_clamp = false;

	const auto& weight_tab = astc_helpers::g_dequant_tables.get_weight_tab(minimum<uint32_t>(astc_helpers::BISE_32_LEVELS, weight_ise_range));

	const uint32_t num_actual_comps = (cem == 4) ? 2 : 1;

	const uint8_t* pSrc_weights = pWeights;
	for (uint32_t y = 0; y < pixels.m_height; y++)
	{
		for (uint32_t x = 0; x < pixels.m_width; x++)
		{
			const uint32_t qw = *pSrc_weights++;
			const int dw = (weight_ise_range <= astc_helpers::BISE_32_LEVELS) ? weight_tab.get_rank_to_val(qw) : qw;
			const float w = dw * (1.0f / 64.0f);

			z00 += w * w;
			z10 += (1.0f - w) * w;
			z11 += (1.0f - w) * (1.0f - w);

			for (uint32_t c = 0; c < num_actual_comps; c++)
			{
				const float a = pixelbuf_get_comp(pixels.m_pBuf, x, y, c ? 3 : 0);

				q00_a[c] += w * a;
				t_a[c] += a;
			}
		} // x
	} // y

	float q10_a[2];
	for (uint32_t c = 0; c < num_actual_comps; c++)
		q10_a[c] = t_a[c] - q00_a[c];

	z01 = z10;

	// default A bounds for cem 0
	pNew_CEM_vals[6] = 255.0f;
	pNew_CEM_vals[7] = 255.0f;

	float det = z00 * z11 - z01 * z10; // should be non-negative mathematically
	//if (fabs(det) >= TINY_EPS)
	if (det >= TINY_EPS)
	{
		det = 1.0f / det;

		float iz00 = z11 * det;
		float iz01 = -z01 * det;
		float iz10 = -z10 * det;
		float iz11 = z00 * det;

		for (uint32_t c = 0; c < num_actual_comps; c++)
		{
			float l = clamp_flag(iz10 * q00_a[c] + iz11 * q10_a[c], 0.0f, 255.0f, did_clamp);
			float h = clamp_flag(iz00 * q00_a[c] + iz01 * q10_a[c], 0.0f, 255.0f, did_clamp);

			if (c == 0)
			{
				pNew_CEM_vals[0] = l;
				pNew_CEM_vals[1] = h;
			}
			else
			{
				pNew_CEM_vals[6] = l;
				pNew_CEM_vals[7] = h;
			}
		}
	}
	else
	{
		const float one_over_total_pixels = 1.0f / (float)(pixels.m_width * pixels.m_height);

		for (uint32_t c = 0; c < num_actual_comps; c++)
		{
			float l = t_a[c] * one_over_total_pixels;
			float h = l;

			if (c == 0)
			{
				pNew_CEM_vals[0] = l;
				pNew_CEM_vals[1] = h;
			}
			else
			{
				pNew_CEM_vals[6] = l;
				pNew_CEM_vals[7] = h;
			}
		}
	}

	// set G and B to R bounds
	pNew_CEM_vals[2] = pNew_CEM_vals[0];
	pNew_CEM_vals[3] = pNew_CEM_vals[1];

	pNew_CEM_vals[4] = pNew_CEM_vals[0];
	pNew_CEM_vals[5] = pNew_CEM_vals[1];

	return did_clamp;
}

// true if clamping occured on outputs
static bool refine_endpoints_given_weights(
	const pixelbuf& pixels,
	uint32_t cem, const uint8_t* pWeights, uint32_t weight_ise_range,
	[[maybe_unused]] const uint8_t* pCEM_values, [[maybe_unused]] uint32_t endpoint_ise_range,
	float pNew_CEM_vals[8],
	uint32_t num_comps)
{
	assert(is_cem_0_or_4(cem) || is_cem_6_or_10(cem) || is_cem_8_or_12(cem));

	switch (cem)
	{
	case 0:
	case 4:
	{
		assert(is_cem_0_or_4(cem));

		return refine_endpoints_given_weights_cem_0_or_4(
			pixels, cem,
			pWeights, weight_ise_range,
			pNew_CEM_vals,
			num_comps);
	}
	case 6:
	case 10:
	{
#if 1
		// root finding, creates new endpoints
		return refine_endpoints_given_weights_cem_6_or_10_method3(
			pixels, cem,
			pWeights, weight_ise_range,
			pNew_CEM_vals,
			num_comps);
#else
		// coordinate descent, needs current endpoints
		return refine_endpoints_given_weights_cem_6_or_10_method2(
			pixels, cem,
			pWeights, weight_ise_range, pCEM_values, endpoint_ise_range,
			pNew_CEM_vals,
			num_comps);
#endif
	}
	case 8:
	case 9:
	case 12:
	case 13:
	{
		return refine_endpoints_given_weights_cem_8_9_12_or_13(
			pixels, cem,
			pWeights, weight_ise_range,
			pNew_CEM_vals,
			num_comps);
	}
	default:
		assert(0);
		break;
	}

	return false;
}

// ccs component was swizzled into A
static void eval_weights_second_plane(
	const pixelbuf& pixels,
	uint32_t cem, uint32_t ccs_index,
	uint8_t* pWeights0, uint8_t* pWeights1, uint32_t weight_ise_range,
	float desired_cem_endpoints[8], // in CEM order
	const uint8_t* pCEM_values, uint32_t endpoint_ise_range,
	uint8_t* pNew_CEM_values,
	uint32_t num_pca_comps, bool ccs_chan_rotated_flag, 
	const single_subset_enc_context& ctx) // true if channel ccs_index was rotated vs. alpha, ccs_index should be <= 2
{
	assert(!is_cem_9_or_13(cem));
	BASISU_NOTE_UNUSED(num_pca_comps);

	float lo_v = 1e+30f, hi_v = -1e+30f;

	// ccs_chan_rotated_flag ccs chan was rotated into alpha, otherwise there was no ccs channel swapping
	const uint32_t actual_src_ccs_chan = ccs_chan_rotated_flag ? 3 : ccs_index;

	for (uint32_t y = 0; y < pixels.m_height; y++)
	{
		for (uint32_t x = 0; x < pixels.m_width; x++)
		{
			float v = pixelbuf_get_comp(pixels.m_pBuf, x, y, actual_src_ccs_chan);
			lo_v = minimum(lo_v, v);
			hi_v = maximum(hi_v, v);
		} // x
	} // y

	// be aware lo_v/hi_v are not clamped 

	if (ccs_index == 3)
	{
		// second plane controls A: insert alpha range directly into the encoded CEM values and we're done
		int lo_v_q = quant_endpoint_val_to_rank(lo_v, endpoint_ise_range, astc_helpers::get_ise_levels(endpoint_ise_range));
		int hi_v_q = quant_endpoint_val_to_rank(hi_v, endpoint_ise_range, astc_helpers::get_ise_levels(endpoint_ise_range));

		if (cem == 10)
		{
			if (pNew_CEM_values != pCEM_values)
				memcpy(pNew_CEM_values, pCEM_values, 4);

			pNew_CEM_values[4] = (uint8_t)lo_v_q;
			pNew_CEM_values[5] = (uint8_t)hi_v_q;

			desired_cem_endpoints[4] = lo_v;
			desired_cem_endpoints[5] = hi_v;
		}
		else if (cem == 12)
		{
			if (pNew_CEM_values != pCEM_values)
				memcpy(pNew_CEM_values, pCEM_values, 6);

			pNew_CEM_values[6] = (uint8_t)lo_v_q;
			pNew_CEM_values[7] = (uint8_t)hi_v_q;

			desired_cem_endpoints[6] = lo_v;
			desired_cem_endpoints[7] = hi_v;
		}
		else if (cem == 4)
		{
			if (pNew_CEM_values != pCEM_values)
				memcpy(pNew_CEM_values, pCEM_values, 2);

			pNew_CEM_values[2] = (uint8_t)lo_v_q;
			pNew_CEM_values[3] = (uint8_t)hi_v_q;

			desired_cem_endpoints[2] = lo_v;
			desired_cem_endpoints[3] = hi_v;
		}
		else
		{
			assert(0);
		}
	}
	else if (is_cem_8_or_12(cem)) //((cem == 8) || (cem == 12))
	{
		assert(num_pca_comps == 3);

		// ccs_index is [0,2], cem is 8/12
		// ccs was swizzled with A, undo that, insert pixel bounds of ccs channel into correct endpoint channel
		assert(ccs_chan_rotated_flag);

		desired_cem_endpoints[3 * 2 + 0] = lo_v;
		desired_cem_endpoints[3 * 2 + 1] = hi_v;

		std::swap(desired_cem_endpoints[3 * 2 + 0], desired_cem_endpoints[ccs_index * 2 + 0]);
		std::swap(desired_cem_endpoints[3 * 2 + 1], desired_cem_endpoints[ccs_index * 2 + 1]);

		cem_encode(cem, desired_cem_endpoints, endpoint_ise_range, pNew_CEM_values, true, ctx.m_higher_effort_bc);
	}
	else
	{
		assert(is_cem_6_or_10(cem));

		// ccs_index is [0,2], cem is 6/10 - no change to endpoints, we're just going to split out one RGB channel into a sep weight plane
		if (pNew_CEM_values != pCEM_values)
			memcpy(pNew_CEM_values, pCEM_values, astc_helpers::get_num_cem_values(cem));

		// no change to desired_cem_endpoints
	}

	float final_endpoints[8];
	cem_decode(cem, pNew_CEM_values, endpoint_ise_range, final_endpoints, nullptr);

	const uint32_t num_weight_levels = astc_helpers::get_ise_levels(weight_ise_range);
	assert(num_weight_levels <= 32);

	const float l = final_endpoints[0 + ccs_index];
	const float h = final_endpoints[4 + ccs_index];
	const float scale = (float)(num_weight_levels - 1) / ((h - l) + TINY_EPS);

	uint8_t* pDst_weights = pWeights1;
	for (uint32_t y = 0; y < pixels.m_height; y++)
	{
		for (uint32_t x = 0; x < pixels.m_width; x++)
		{
			const float v = pixelbuf_get_comp(pixels.m_pBuf, x, y, actual_src_ccs_chan);

			const int iw = clamp<int>((int)((v - l) * scale + .5f), 0, num_weight_levels - 1);

			*pDst_weights++ = (uint8_t)iw;
		} // x
	} // y

	if ((ccs_index < 3) && (is_cem_8_or_12(cem)))
	{
		assert(ccs_chan_rotated_flag);

		// CEM 8 or 12.
		// We re-encoded the endpoints, so update the weights in case they changed.
		// Final endpoints are not CCS swapped (rotated), so we'll need eval_weights_first_plane() to swap them on 
		// endpoint decode to match the input pixels here which are swapped with A.
		eval_weights_first_plane(pixels,
			pWeights0, weight_ise_range, cem,
			pNew_CEM_values, endpoint_ise_range,
			3, ccs_index);
	}
}

static void try_base_ofs_cem(
	uint32_t cem_index,
	const pixelbuf& block,
	astc_helpers::log_astc_block& log_blk,
	[[maybe_unused]] const single_subset_enc_context& ctx,
	[[maybe_unused]] uint32_t num_block_comps, // 3 or 4, for 3 alpha=255
	float desired_cem_endpoints[8], 
	bool ccs_chan_rotated_flag) // if true, a and the ccs chan are swapped in block's pixels
{
	assert(!is_lblock_ise(log_blk));

	if (is_cem_9_or_13(log_blk.m_color_endpoint_modes[0]))
		return;

	assert(is_cem_8_or_12(cem_index));

	if (log_blk.m_endpoint_ise_range == astc_helpers::BISE_256_LEVELS)
		return;

	// try base+ofs, very rare win (~1-3% vs. direct)
	uint8_t CEM_values_base_ofs[8];

	const uint32_t cem_index_base_ofs = (cem_index == 8) ? 9 : 13;
	const uint32_t num_comps = get_num_cem_chans(cem_index);
	const uint32_t num_base_ofs_cem_vals = astc_helpers::get_num_cem_values(cem_index_base_ofs);

	const bool base_ofs_fit = encode_cem_9_13(cem_index_base_ofs, CEM_values_base_ofs, log_blk.m_endpoint_ise_range, desired_cem_endpoints, true);
	if (!base_ofs_fit)
		return;

	const auto& endpoint_tab = astc_helpers::g_dequant_tables.get_endpoint_tab(log_blk.m_endpoint_ise_range);
	for (uint32_t i = 0; i < num_base_ofs_cem_vals; i++)
		CEM_values_base_ofs[i] = endpoint_tab.m_ISE_to_rank[CEM_values_base_ofs[i]];

	float ep_base_ofs[8];
	cem_decode(cem_index_base_ofs, CEM_values_base_ofs, log_blk.m_endpoint_ise_range, ep_base_ofs, nullptr);

	float ep_direct[8];
	cem_decode(cem_index, log_blk.m_endpoints, log_blk.m_endpoint_ise_range, ep_direct, nullptr);
		
	float desired_endpoints_lh_rgba[8];

	for (uint32_t c = 0; c < num_comps; c++)
	{
		desired_endpoints_lh_rgba[c + 0] = desired_cem_endpoints[c * 2 + 0];
		desired_endpoints_lh_rgba[c + 4] = desired_cem_endpoints[c * 2 + 1];
	}

	const float dist_base_ofs = calc_shortest_endpoint_dist(desired_endpoints_lh_rgba, ep_base_ofs, num_comps);
	const float dist_direct = calc_shortest_endpoint_dist(desired_endpoints_lh_rgba, ep_direct, num_comps);

	if (dist_base_ofs < dist_direct)
	{
		log_blk.m_color_endpoint_modes[0] = (uint8_t)cem_index_base_ofs;
		memcpy(log_blk.m_endpoints, CEM_values_base_ofs, num_base_ofs_cem_vals);
				
		const uint32_t weight1_chan_mask = log_blk.m_dual_plane ? (1u << log_blk.m_color_component_selector) : 0;
		const uint32_t weight0_chan_mask = ((1 << num_comps) - 1) ^ weight1_chan_mask;
		
		eval_weights_for_plane(
			block,
			log_blk.m_dual_plane ? 2 : 1,
			log_blk.m_weights, log_blk.m_weight_ise_range,
			log_blk.m_color_endpoint_modes[0], log_blk.m_endpoints, log_blk.m_endpoint_ise_range,
			weight0_chan_mask, ccs_chan_rotated_flag ? log_blk.m_color_component_selector : -1);

		if (log_blk.m_dual_plane)
		{
			eval_weights_for_plane(
				block,
				2,
				log_blk.m_weights + 1, log_blk.m_weight_ise_range,
				log_blk.m_color_endpoint_modes[0], log_blk.m_endpoints, log_blk.m_endpoint_ise_range,
				weight1_chan_mask, ccs_chan_rotated_flag ? log_blk.m_color_component_selector : -1);
		}
	}
}

// PCA comps
// single plane:
//   cem 0: 3D (R=G=B, really 1D)
//   cem 4: 4D (R=G=B + A, really 2D)
//   cem 6: 3D, zero centered
//   cem 8: 3D
//   cem 10: 4D, zero centered
//   cem 12: 4D 
// dual plane, CCS=3:
//   cem 0: invalid
//   cem 4: 3D (R=G=B + A, really 1D)
//   cem 6: invalid
//   cem 8: invalid
//   cem 10: 3D, zero centered
//   cem 12: 3D
// dual plane, CCS=[0,2] - for CEM 6/10, solve as SP, then introduce 2nd weight plane but don't change the encoded RGBA endpoints
//   cem 0: invalid
//   cem 4: invalid
//   cem 6: 3D zero centered, unrotated pixels (i.e. ccs channel NOT swapped with A)
//   cem 8: 3D, rotated pixels (essentially 2D PCA as all A=255)
//   cem 10: 4D, zero centered, unrotated pixels (exceptional case)
//   cem 12: 3D, rotated pixels
static inline void get_pca_config(const astc_unpacked_config& cfg, uint32_t& num_pca_comps, bool& ccs_chan_rotated_flag)
{
	num_pca_comps = 3;
	ccs_chan_rotated_flag = false;

	if (cfg.m_dual_plane)
	{
		assert(cfg.m_cem != 0);

		if (cfg.m_cem == 4)
		{
			assert(cfg.m_ccs_index == 3);
			//num_pca_comps = 3;
		}
		else if (cfg.m_ccs_index < 3)
		{
			num_pca_comps = (cfg.m_cem == 10) ? 4 : 3;
			ccs_chan_rotated_flag = is_cem_8_or_12(cfg.m_cem); // ((cfg.m_cem & 3) == 0); // cem 8 or 12, otherwise not ccs channel rotated with A
		}
	}
	else
	{
		num_pca_comps = ((cfg.m_cem == 4) || (cfg.m_cem >= 10)) ? 4 : 3;
	}
}

static void create_single_subset_block(
	const pixelbuf& block,
	astc_helpers::log_astc_block& log_blk,
	const single_subset_enc_context& ctx,
	uint32_t num_block_comps, // 3 or 4, for 3 alpha=255
	uint32_t num_pca_comps, // 3D or 4D
	bool ccs_chan_rotated_flag,
	bool try_base_ofs) // ccs_chan_rotated_flag is true for dual plane if the CCS channel has been swapped/rotated with alpha
{
	const uint32_t cem_index = log_blk.m_color_endpoint_modes[0];
	
	assert(is_cem_0_or_4(cem_index) || is_cem_6_or_10(cem_index) || is_cem_8_or_12(cem_index));

	const bool cem_6_or_10 = is_cem_6_or_10(cem_index); // (cem_index & 3) == 2;
	//const bool cem_has_alpha = does_cem_have_alpha(cem_index); // only true when the block has alpha, too
	const bool dual_plane = log_blk.m_dual_plane;

	if (num_block_comps == 3)
	{
		assert((cem_index == 0) || (cem_index == 6) || (cem_index == 8));
	}

	if (dual_plane)
	{
		if (log_blk.m_color_component_selector == 3)
		{
			assert((cem_index == 4) || (cem_index >= 10));
		}
		else 
		{
			assert(!is_cem_0_or_4(cem_index));
		}
	}

	float desired_cem_endpoints[8];
	calc_initial_cem_endpoints(block, desired_cem_endpoints, num_pca_comps, nullptr, cem_6_or_10);

	cem_encode(cem_index, desired_cem_endpoints, log_blk.m_endpoint_ise_range, log_blk.m_endpoints, true, ctx.m_higher_effort_bc);

	eval_weights_first_plane(block, log_blk.m_weights, log_blk.m_weight_ise_range,
		cem_index, log_blk.m_endpoints, log_blk.m_endpoint_ise_range,
		num_pca_comps);

	for (uint32_t i = 0; i < ctx.m_num_ls_iterations; i++)
	{
		refine_endpoints_given_weights(block, cem_index, log_blk.m_weights, log_blk.m_weight_ise_range, log_blk.m_endpoints, log_blk.m_endpoint_ise_range, desired_cem_endpoints, num_pca_comps);
				
		cem_encode(cem_index, desired_cem_endpoints, log_blk.m_endpoint_ise_range, log_blk.m_endpoints, true, ctx.m_higher_effort_bc);

		eval_weights_first_plane(block, log_blk.m_weights, log_blk.m_weight_ise_range,
			cem_index, log_blk.m_endpoints, log_blk.m_endpoint_ise_range,
			num_pca_comps);
	} // i

	if (dual_plane)
	{
		uint8_t weights[2][astc_helpers::MAX_GRID_WEIGHTS];

		eval_weights_second_plane(
			block,
			cem_index, log_blk.m_color_component_selector,
			log_blk.m_weights, weights[1], log_blk.m_weight_ise_range,
			desired_cem_endpoints,
			log_blk.m_endpoints, log_blk.m_endpoint_ise_range,
			log_blk.m_endpoints,
			num_pca_comps, ccs_chan_rotated_flag, ctx);

		const uint32_t num_grid_samples = log_blk.m_grid_width * log_blk.m_grid_height;
		memcpy(weights[0], log_blk.m_weights, num_grid_samples);

		for (uint32_t i = 0; i < num_grid_samples; i++)
		{
			log_blk.m_weights[i * 2 + 0] = weights[0][i];
			log_blk.m_weights[i * 2 + 1] = weights[1][i];
		} // i
	}

	if ((try_base_ofs) && ((cem_index == 8) || (cem_index == 12)))
	{
		try_base_ofs_cem(cem_index,
			block,
			log_blk,
			ctx,
			num_block_comps,
			desired_cem_endpoints,
			ccs_chan_rotated_flag);
	}
}

static void encode_single_subset_block(
	const single_subset_enc_context& ctx,
	const pixelbuf& src_block, uint32_t num_src_block_comps,
	const astc_unpacked_config& cfg,
	astc_helpers::log_astc_block& log_blk,
	bool try_base_ofs)
{
	assert(cfg.m_grid_width <= ctx.m_block_width);
	assert(cfg.m_grid_height <= ctx.m_block_height);

	log_blk.clear();
	log_blk.m_grid_width = cfg.m_grid_width;
	log_blk.m_grid_height = cfg.m_grid_height;
	log_blk.m_endpoint_ise_range = cfg.m_endpoint_range;
	log_blk.m_weight_ise_range = cfg.m_weight_range;
	log_blk.m_dual_plane = cfg.m_dual_plane;
	log_blk.m_color_component_selector = cfg.m_ccs_index;
	log_blk.m_num_partitions = 1;
	log_blk.m_color_endpoint_modes[0] = cfg.m_cem;
	log_blk.m_user_mode = cUserModeRankValues;

	if (num_src_block_comps == 3)
	{
		//assert(cfg.m_cem <= 8);
		assert((cfg.m_cem == 0) || (cfg.m_cem == 6) || (cfg.m_cem == 8));
	}
	
	uint32_t num_pca_comps;
	bool ccs_chan_rotated_flag;
	get_pca_config(cfg, num_pca_comps, ccs_chan_rotated_flag);

	float grid_pixels[PIXELBUF_SIZE_IN_FLOATS];
	pixelbuf grid_pbuf(cfg.m_grid_width, cfg.m_grid_height, grid_pixels);

	const uint32_t num_block_chans_to_encode = minimum<uint32_t>(get_num_cem_chans(cfg.m_cem), num_src_block_comps); // always 3 or 4

	pseudoinverse_block_to_grid(src_block, grid_pbuf, num_block_chans_to_encode);

	if (num_block_chans_to_encode == 3)
		pixelbuf_set_comp_to_val(grid_pbuf, 3, 255.0f);

	if (ccs_chan_rotated_flag)
		pixelbuf_swap_comp_with_alpha(grid_pbuf, cfg.m_ccs_index);

	create_single_subset_block(grid_pbuf, log_blk, ctx, num_block_chans_to_encode, num_pca_comps, ccs_chan_rotated_flag, try_base_ofs);
}

bool init_single_subset_context(
	single_subset_enc_context& ctx,
	uint32_t block_width, uint32_t block_height,
	astc_helpers::decode_mode astc_decode_mode,
	const uint32_t chan_weights[4],
	uint32_t max_candidates, uint32_t num_ls_iterations, bool disable_dual_plane, bool has_alpha, bool weight_polishing)
{
	const int idx = astc_helpers::find_astc_block_size_index(block_width, block_height);
	assert(idx != -1);
	if (idx == -1)
	{
		assert(0);
		return false;
	}

	if (max_candidates > MAX_CANDIDATES)
	{
		assert(0);
		return false;
	}

	ctx.m_block_width = block_width;
	ctx.m_block_height = block_height;
		
	ctx.m_block_size_index = idx;
	ctx.m_total_block_pixels = block_width * block_height;
	
	ctx.m_astc_decode_mode = astc_decode_mode;

	memcpy(ctx.m_chan_weights, chan_weights, sizeof(ctx.m_chan_weights));
		
	ctx.m_max_candidates = max_candidates;
	ctx.m_num_ls_iterations = num_ls_iterations;
	ctx.m_weight_polishing = weight_polishing;

	ctx.m_disable_dual_plane = disable_dual_plane;
	ctx.m_has_alpha = has_alpha;

	ctx.m_try_base_ofs = true;
	ctx.m_higher_effort_bc = true;
		
	return ctx.m_dct.init(block_height, block_width); // rows=block height, cols=block width
}

static inline float calc_weighted_error(const astc_helpers::color_rgba& c, float r, float g, float b, float a, const single_subset_enc_context& state)
{
	return (square((float)c.m_r - r) * (float)state.m_chan_weights[0]) +
		(square((float)c.m_g - g) * (float)state.m_chan_weights[1]) +
		(square((float)c.m_b - b) * (float)state.m_chan_weights[2]) +
		(square((float)c.m_a - a) * (float)state.m_chan_weights[3]);
}

// 1-3 subsets
// returns true if lblock_to_refine was changed
static bool weight_polish(
	const pixelbuf& src_block,
	astc_helpers::log_astc_block& lblock_to_refine,
	const single_subset_enc_context& enc_state,
	const basist::astc_ldr_t::astc_block_grid_data* pGrid_data,
	double& best_lblock_error, astc_helpers::log_astc_block& best_lblock,
	astc_lblock_vec* pAll_candidates)
{
	assert((lblock_to_refine.m_num_partitions >= 1) && (lblock_to_refine.m_num_partitions <= 3));
		
	const uint32_t block_width = enc_state.m_block_width, block_height = enc_state.m_block_height;
	[[maybe_unused]] const uint32_t total_block_pixels = block_width * block_height;

	[[maybe_unused]] const uint32_t num_cem_endpoint_vals = astc_helpers::get_num_cem_values(lblock_to_refine.m_color_endpoint_modes[0]);
	[[maybe_unused]] const uint32_t total_grid_weights = lblock_to_refine.m_grid_width * lblock_to_refine.m_grid_height;

	[[maybe_unused]] const auto& endpoint_tab = astc_helpers::g_dequant_tables.get_endpoint_tab(lblock_to_refine.m_endpoint_ise_range);
	const auto& weight_tab = astc_helpers::g_dequant_tables.get_weight_tab(lblock_to_refine.m_weight_ise_range);

	const uint32_t num_weight_levels = astc_helpers::get_ise_levels(lblock_to_refine.m_weight_ise_range);

	[[maybe_unused]] const uint32_t num_partitions = lblock_to_refine.m_num_partitions;

	astc_helpers::log_astc_block refined_lblock_ise(lblock_to_refine);
	convert_rank_lblock_to_ise(refined_lblock_ise); // xuastc_ldr_block_decoder expects ISE blocks

	bool changed_flag = false;

	const uint32_t grid_width = refined_lblock_ise.m_grid_width, grid_height = refined_lblock_ise.m_grid_height;

	astc_helpers::xuastc_ldr_block_decoder block_decoder;
	block_decoder.init(refined_lblock_ise, block_width, block_height, enc_state.m_astc_decode_mode, pGrid_data->m_upsample_weights.get_ptr());

#if defined(DEBUG) || defined(_DEBUG)
	const double error1 = compute_block_error(refined_lblock_ise, src_block, enc_state);
#endif

	for (uint32_t y = 0; y < grid_height; y++)
	{
		for (uint32_t x = 0; x < grid_width; x++)
		{
			const uint32_t idx = x + y * grid_width;

			const basisu::uint16_vec& influenced_texels = pGrid_data->m_grid_to_texel_influence_list[idx];

			int cur_rank = weight_tab.m_ISE_to_rank[refined_lblock_ise.m_weights[idx]];

			const uint8_t orig_weight_ise = astc_helpers::get_weight(refined_lblock_ise, 0, idx);

			double best_err = 0.0f;

			for (uint32_t j = 0; j < influenced_texels.size(); j++)
			{
				const uint32_t packed_texel_index = influenced_texels[j];
				const uint32_t texel_x = packed_texel_index & 0xFF;
				const uint32_t texel_y = packed_texel_index >> 8;
				
				astc_helpers::color_rgba dec_c;
				block_decoder.decode_texel(texel_x, texel_y, dec_c);

				best_err += calc_weighted_error(dec_c, pixelbuf_get_comp(src_block.m_pBuf, texel_x, texel_y, 0), pixelbuf_get_comp(src_block.m_pBuf, texel_x, texel_y, 1), pixelbuf_get_comp(src_block.m_pBuf, texel_x, texel_y, 2), pixelbuf_get_comp(src_block.m_pBuf, texel_x, texel_y, 3), enc_state);
			}

			if (best_err == 0.0f)
				continue;
						
			uint8_t best_weight_ise = orig_weight_ise;

			for (int idir = -1; idir <= 1; idir += 2)
			{
				const int new_rank = clamp(cur_rank + idir, 0, (int)num_weight_levels - 1);
				const uint32_t new_ise = weight_tab.m_rank_to_ISE[new_rank];
								
				// change current weight
				astc_helpers::get_weight(refined_lblock_ise, 0, idx) = (uint8_t)new_ise;

				double trial_err = 0.0f;
				for (uint32_t j = 0; j < influenced_texels.size(); j++)
				{
					const uint32_t packed_texel_index = influenced_texels[j];
					const uint32_t texel_x = packed_texel_index & 0xFF;
					const uint32_t texel_y = packed_texel_index >> 8;
					//const uint32_t texel_index = texel_x + texel_y * block_width;

					astc_helpers::color_rgba dec_c;
					block_decoder.decode_texel(texel_x, texel_y, dec_c);

					trial_err += calc_weighted_error(dec_c, pixelbuf_get_comp(src_block.m_pBuf, texel_x, texel_y, 0), pixelbuf_get_comp(src_block.m_pBuf, texel_x, texel_y, 1), pixelbuf_get_comp(src_block.m_pBuf, texel_x, texel_y, 2), pixelbuf_get_comp(src_block.m_pBuf, texel_x, texel_y, 3), enc_state);
				}

				astc_helpers::get_weight(refined_lblock_ise, 0, idx) = orig_weight_ise;

				if (trial_err < best_err)
				{
					// accept
					best_err = trial_err;
					best_weight_ise = (uint8_t)new_ise;
					changed_flag = true;
					if (best_err == 0.0f)
						break;
				}

			} // idir

			astc_helpers::get_weight(refined_lblock_ise, 0, idx) = best_weight_ise;

		} // x
	} //y

#if defined(DEBUG) || defined(_DEBUG)
	const double error2 = compute_block_error(refined_lblock_ise, src_block, enc_state);
	assert(error2 <= error1);
#endif

	convert_ise_lblock_to_rank(refined_lblock_ise);
	// refined_lblock_ise is now in rank space

	if (changed_flag)
	{
		const double refined_error = compute_block_error(refined_lblock_ise, src_block, enc_state);
		if (refined_error < best_lblock_error)
		{
			best_lblock_error = refined_error;
			memcpy(&best_lblock, &refined_lblock_ise, sizeof(best_lblock));

		}

		if (pAll_candidates)
			pAll_candidates->push_back(refined_lblock_ise);

		lblock_to_refine = refined_lblock_ise;
	}

	return changed_flag;
}

static double compress_single_subset_internal(
	const single_subset_enc_context& ctx,
	const uint8_t* pBlock_pixels, 
	astc_helpers::log_astc_block& best_lblock, 
	astc_lblock_vec* pAll_candidates,
	single_subset_shortlist_state& shortlist_state,
	bool always_compute_error, float scale_weight)
{
#if 0
	{
		basisu::rand rnd;
		rnd.seed(1115);
		for (; ; )
		{
			float endpoints[6];
			for (uint32_t i = 0; i < 6; i++)
				endpoints[i] = rnd.frand(-4.0f, 255.0f + 4);

			int range = rnd.irand(astc_helpers::FIRST_VALID_ENDPOINT_ISE_RANGE, astc_helpers::LAST_VALID_ENDPOINT_ISE_RANGE);
			
			uint8_t cem_vals[6];
			encode_cem_8_12(8, cem_vals, range, endpoints);
		}
	}
#endif

	assert(ctx.m_block_height <= 12);
	assert(ctx.m_block_width <= 12);
	assert(ctx.m_total_block_pixels == (ctx.m_block_width * ctx.m_block_height));
	assert(ctx.m_max_candidates && (ctx.m_max_candidates <= MAX_CANDIDATES));
		
	const uint32_t* pBlock_pixels_u32 = reinterpret_cast<const uint32_t*>(pBlock_pixels);

	const uint32_t first_pixel = pBlock_pixels_u32[0];

	const int last_pixel_index = ctx.m_total_block_pixels - 1;

	if (pBlock_pixels_u32[last_pixel_index] == first_pixel)
	{
		int i;
		for (i = 1; i < last_pixel_index; i++)
			if (pBlock_pixels_u32[i] != first_pixel)
				break;

		if (i == last_pixel_index)
		{
			astc_helpers::set_ldr_solid_block(best_lblock, pBlock_pixels[0], pBlock_pixels[1], pBlock_pixels[2], pBlock_pixels[3]);

			if (pAll_candidates)
				pAll_candidates->push_back(best_lblock);

			return 0.0f;
		}
	}
		
	rgba32_image block_img;
	block_img.m_pPixels = pBlock_pixels;
	block_img.m_width = ctx.m_block_width;
	block_img.m_height = ctx.m_block_height;
	block_img.m_row_pitch_in_texels = ctx.m_block_width;
			
	uint32_t num_src_block_comps = 3;
	bool src_is_luma_only = true;

	if (ctx.m_has_alpha)
	{
		for (uint32_t i = 0; i < ctx.m_total_block_pixels; i++)
		{
			const uint8_t r = pBlock_pixels[i * 4 + 0];
			const uint8_t g = pBlock_pixels[i * 4 + 1];
			const uint8_t b = pBlock_pixels[i * 4 + 2];
			const uint8_t a = pBlock_pixels[i * 4 + 3];

			if ((r != g) || (r != b))
				src_is_luma_only = false;

			if (a != 255)
				num_src_block_comps = 4;
		}
	}
	else
	{
		for (uint32_t i = 0; i < ctx.m_total_block_pixels; i++)
		{
			const uint8_t r = pBlock_pixels[i * 4 + 0];
			const uint8_t g = pBlock_pixels[i * 4 + 1];
			const uint8_t b = pBlock_pixels[i * 4 + 2];

			if ((r != g) || (r != b))
			{
				src_is_luma_only = false;
				break;
			}
		}
	}

	uint32_t total_packed_configs = TOTAL_SINGLE_SUBSET_CONFIGS_RGBA;
	const uint32_t* pPacked_configs = g_single_subset_configs_rgba;

	if (src_is_luma_only)
	{
		total_packed_configs = TOTAL_SINGLE_SUBSET_CONFIGS_LA;
		pPacked_configs = g_single_subset_configs_la;
	}

	init_single_subset_shortlist_state(block_img, ctx, shortlist_state, src_is_luma_only, num_src_block_comps);
					
	const uint32_t total_candidates = generate_single_subset_shortlist(total_packed_configs, pPacked_configs, ctx, block_img, src_is_luma_only, 
		num_src_block_comps, shortlist_state, scale_weight, ctx.m_max_candidates);

	if (!total_candidates)
	{
		astc_helpers::set_ldr_solid_block(best_lblock, 0xFF, 0, 0xFF, 0xFF);

		if (pAll_candidates)
			pAll_candidates->push_back(best_lblock);

		assert(0);
		return DBL_MAX;
	}

	double best_err = DBL_MAX;
		
	for (uint32_t cand_index = 0; cand_index < total_candidates; cand_index++)
	{
		astc_helpers::log_astc_block trial_lblock;

		encode_single_subset_block(ctx, shortlist_state.m_pbuf, num_src_block_comps, shortlist_state.m_best_configs[cand_index], trial_lblock, ctx.m_try_base_ofs);

		if (pAll_candidates)
			pAll_candidates->push_back(trial_lblock);

		if (always_compute_error || (total_candidates > 1))
		{
			double err = compute_block_error(trial_lblock, shortlist_state.m_pbuf, ctx);
			if (err < best_err)
			{
				best_err = err;
				best_lblock = trial_lblock;
			}
		}
		else
		{
			best_err = -1.0f;
			best_lblock = trial_lblock;
		}
	}

	if (ctx.m_weight_polishing)
	{
		const basist::astc_ldr_t::astc_block_grid_data* pGrid_data = basist::astc_ldr_t::find_astc_block_grid_data(
			ctx.m_block_width, ctx.m_block_height, best_lblock.m_grid_width, best_lblock.m_grid_height);

		weight_polish(shortlist_state.m_pbuf, best_lblock, ctx, pGrid_data, best_err, best_lblock, pAll_candidates);
	}

	convert_rank_lblock_to_ise(best_lblock);

	return best_err;
}

double compress_single_subset(
	single_subset_enc_context& ctx,
	const uint8_t* pBlock_pixels,
	astc_helpers::log_astc_block& best_lblock,
	astc_lblock_vec* pAll_candidates,
	bool always_compute_error)
{
	single_subset_shortlist_state shortlist_state;

	return compress_single_subset_internal(ctx, pBlock_pixels, best_lblock, pAll_candidates, shortlist_state, always_compute_error, DEF_SCALE_WEIGHT);
}

// unpack_config() compatible
// cems 6,8,10,12 only, gw>=gh (opposite case of gw<gh handled in code), dp ccs index handled in code
const uint32_t TOTAL_TWO_SUBSET_CONFIGS_RGBA = 542;
static const uint32_t g_two_subset_configs_rgba[TOTAL_TWO_SUBSET_CONFIGS_RGBA] =
{
	0x14556, 0x14466, 0x14566, 0x13666, 0x14476, 0x14576, 0x11676, 0xe776, 0x14386, 0x14486, 0x12586, 0xf686, 0xc786, 0x9886, 0xe558, 0xe468, 0xd568, 0xb668, 0xd478, 0xc578, 0xa678, 0x8778, 0xe388, 0xc488, 0xa588, 0x8688, 0x6788, 0x4888, 0xe55a, 0xe46a, 0xd56a, 0xb66a,
	0xd47a, 0xc57a, 0xa67a, 0x877a, 0xe38a, 0xc48a, 0xa58a, 0x868a, 0x678a, 0x488a, 0xa55c, 0xa46c, 0x856c, 0x766c, 0x947c, 0x857c, 0x667c, 0x577c, 0xa38c, 0x848c, 0x758c, 0x568c, 0x478c, 0x34446, 0x34356, 0x34456, 0x32556, 0x34366, 0x32466, 0x2f566, 0x2b666, 0x34376,
	0x30476, 0x2c576, 0x27676, 0x34286, 0x32386, 0x2d486, 0x29586, 0x24686, 0x2e448, 0x2e358, 0x2c458, 0x2a558, 0x2d368, 0x2b468, 0x28568, 0x26668, 0x2c378, 0x29478, 0x26578, 0x2e288, 0x2b388, 0x27488, 0x24588, 0x2e44a, 0x2e35a, 0x2c45a, 0x2a55a, 0x2d36a, 0x2b46a, 0x2856a, 0x2666a, 0x2c37a,
	0x2947a, 0x2657a, 0x2e28a, 0x2b38a, 0x2748a, 0x2458a, 0x2944c, 0x2a35c, 0x2845c, 0x2755c, 0x2936c, 0x2746c, 0x2556c, 0x2837c, 0x2647c, 0x2457c, 0x2928c, 0x2738c, 0x2448c, 0x54346, 0x54446, 0x54356, 0x52456, 0x4e556, 0x54266, 0x53366, 0x4f466, 0x4a566, 0x46666, 0x54276, 0x51376, 0x4c476,
	0x47576, 0x54286, 0x4f386, 0x49486, 0x4e348, 0x4c448, 0x4d358, 0x4a458, 0x48558, 0x4e268, 0x4b368, 0x48468, 0x45568, 0x4d278, 0x4a378, 0x46478, 0x4c288, 0x48388, 0x44488, 0x4e34a, 0x4c44a, 0x4d35a, 0x4a45a, 0x4855a, 0x4e26a, 0x4b36a, 0x4846a, 0x4556a, 0x4d27a, 0x4a37a, 0x4647a, 0x4c28a,
	0x4838a, 0x4448a, 0x4a34c, 0x4844c, 0x4835c, 0x4745c, 0x4555c, 0x4a26c, 0x4736c, 0x4546c, 0x4927c, 0x4637c, 0x4447c, 0x4828c, 0x4538c, 0x74346, 0x73446, 0x74256, 0x74356, 0x6f456, 0x6b556, 0x74266, 0x71366, 0x6c466, 0x67566, 0x74276, 0x6e376, 0x68476, 0x73286, 0x6c386, 0x65486, 0x6d348,
	0x6b448, 0x6e258, 0x6c358, 0x69458, 0x66558, 0x6d268, 0x6a368, 0x66468, 0x6c278, 0x68378, 0x64478, 0x6b288, 0x66388, 0x6d34a, 0x6b44a, 0x6e25a, 0x6c35a, 0x6945a, 0x6655a, 0x6d26a, 0x6a36a, 0x6646a, 0x6c27a, 0x6837a, 0x6447a, 0x6b28a, 0x6638a, 0x6934c, 0x6744c, 0x6a25c, 0x6835c, 0x6545c,
	0x6926c, 0x6636c, 0x6446c, 0x6827c, 0x6537c, 0x6728c, 0x6438c, 0x94336, 0x94346, 0x91446, 0x94256, 0x92356, 0x8d456, 0x88556, 0x94266, 0x8f366, 0x89466, 0x84566, 0x93276, 0x8c376, 0x85476, 0x91286, 0x89386, 0x8e338, 0x8c348, 0x8a448, 0x8e258, 0x8b358, 0x87458, 0x84558, 0x8c268, 0x89368,
	0x85468, 0x8b278, 0x87378, 0x8a288, 0x85388, 0x8e33a, 0x8c34a, 0x8a44a, 0x8e25a, 0x8b35a, 0x8745a, 0x8455a, 0x8c26a, 0x8936a, 0x8546a, 0x8b27a, 0x8737a, 0x8a28a, 0x8538a, 0x8a33c, 0x8834c, 0x8644c, 0x8925c, 0x8735c, 0x8445c, 0x8826c, 0x8536c, 0x8727c, 0x8437c, 0x8628c, 0xb4336, 0xb4246,
	0xb3346, 0xaf446, 0xb4256, 0xb0356, 0xaa456, 0xa5556, 0xb3266, 0xad366, 0xa6466, 0xb1276, 0xa9376, 0xaf286, 0xa6386, 0xae338, 0xae248, 0xab348, 0xa8448, 0xad258, 0xa9358, 0xa5458, 0xab268, 0xa7368, 0xaa278, 0xa5378, 0xa8288, 0xae33a, 0xae24a, 0xab34a, 0xa844a, 0xad25a, 0xa935a, 0xa545a,
	0xab26a, 0xa736a, 0xaa27a, 0xa537a, 0xa828a, 0xa933c, 0xaa24c, 0xa734c, 0xa544c, 0xa825c, 0xa635c, 0xa726c, 0xa436c, 0xa627c, 0xa528c, 0xd4336, 0xd4246, 0xd2346, 0xcd446, 0xd4256, 0xce356, 0xc8456, 0xd2266, 0xca366, 0xcf276, 0xc7376, 0xcd286, 0xcd338, 0xce248, 0xca348, 0xc7448, 0xcc258,
	0xc8358, 0xc4458, 0xca268, 0xc5368, 0xc9278, 0xc7288, 0xcd33a, 0xce24a, 0xca34a, 0xc744a, 0xcc25a, 0xc835a, 0xc445a, 0xca26a, 0xc536a, 0xc927a, 0xc728a, 0xc833c, 0xc924c, 0xc734c, 0xc444c, 0xc825c, 0xc535c, 0xc726c, 0xc527c, 0xc428c, 0xf4336, 0xf4246, 0xf0346, 0xeb446, 0xf3256, 0xed356,
	0xe6456, 0xf0266, 0xe8366, 0xee276, 0xe4376, 0xeb286, 0xec338, 0xed248, 0xe9348, 0xe6448, 0xeb258, 0xe7358, 0xe9268, 0xe4368, 0xe8278, 0xe6288, 0xec33a, 0xed24a, 0xe934a, 0xe644a, 0xeb25a, 0xe735a, 0xe926a, 0xe436a, 0xe827a, 0xe628a, 0xe833c, 0xe924c, 0xe634c, 0xe725c, 0xe435c, 0xe626c,
	0xe527c, 0x114236, 0x113336, 0x114246, 0x10f346, 0x109446, 0x112256, 0x10a356, 0x10f266, 0x106366, 0x10c276, 0x109286, 0x10e238, 0x10b338, 0x10c248, 0x108348, 0x104448, 0x10a258, 0x105358, 0x108268, 0x106278, 0x104288, 0x10e23a, 0x10b33a, 0x10c24a, 0x10834a, 0x10444a, 0x10a25a, 0x10535a, 0x10826a, 0x10627a, 0x10428a,
	0x10a23c, 0x10733c, 0x10824c, 0x10534c, 0x10725c, 0x10526c, 0x10427c, 0x134236, 0x132336, 0x134246, 0x12d346, 0x127446, 0x130256, 0x128356, 0x12d266, 0x124366, 0x12a276, 0x127286, 0x12e238, 0x12b338, 0x12c248, 0x127348, 0x129258, 0x124358, 0x127268, 0x125278, 0x12e23a, 0x12b33a, 0x12c24a, 0x12734a, 0x12925a, 0x12435a,
	0x12726a, 0x12527a, 0x12923c, 0x12733c, 0x12824c, 0x12434c, 0x12625c, 0x12426c, 0x154236, 0x151336, 0x153246, 0x14c346, 0x145446, 0x150256, 0x147356, 0x14c266, 0x148276, 0x145286, 0x14d238, 0x14a338, 0x14b248, 0x146348, 0x149258, 0x146268, 0x144278, 0x14d23a, 0x14a33a, 0x14b24a, 0x14634a, 0x14925a, 0x14626a, 0x14427a,
	0x14923c, 0x14633c, 0x14724c, 0x14434c, 0x14525c, 0x14426c, 0x174236, 0x170336, 0x172246, 0x16a346, 0x16e256, 0x165356, 0x16a266, 0x167276, 0x16d238, 0x169338, 0x16a248, 0x165348, 0x168258, 0x165268, 0x16d23a, 0x16933a, 0x16a24a, 0x16534a, 0x16825a, 0x16526a, 0x16823c, 0x16633c, 0x16724c, 0x16525c
};

const uint32_t TOTAL_THREE_SUBSET_CONFIGS_RGBA = 332;
static const uint32_t g_three_subset_configs_rgba[TOTAL_THREE_SUBSET_CONFIGS_RGBA] =
{
	0xe556, 0xe466, 0xd566, 0xb666, 0xd476, 0xc576, 0xa676, 0x8776, 0xe386, 0xc486, 0xa586, 0x8686, 0x6786, 0x4886, 0x8558, 0x8468, 0x7568, 0x6668, 0x7478, 0x6578, 0x5678, 0x4778, 0x8388, 0x7488, 0x5588, 0x4688, 0x855a, 0x846a, 0x756a, 0x666a, 0x747a, 0x657a,
	0x567a, 0x477a, 0x838a, 0x748a, 0x558a, 0x468a, 0x2e446, 0x2e356, 0x2c456, 0x2a556, 0x2d366, 0x2b466, 0x28566, 0x26666, 0x2c376, 0x29476, 0x26576, 0x2e286, 0x2b386, 0x27486, 0x24586, 0x28448, 0x28358, 0x27458, 0x25558, 0x27368, 0x26468, 0x24568, 0x27378, 0x25478, 0x28288, 0x26388,
	0x24488, 0x2844a, 0x2835a, 0x2745a, 0x2555a, 0x2736a, 0x2646a, 0x2456a, 0x2737a, 0x2547a, 0x2828a, 0x2638a, 0x2448a, 0x4e346, 0x4c446, 0x4d356, 0x4a456, 0x48556, 0x4e266, 0x4b366, 0x48466, 0x45566, 0x4d276, 0x4a376, 0x46476, 0x4c286, 0x48386, 0x44486, 0x48348, 0x47448, 0x47358, 0x45458,
	0x44558, 0x48268, 0x46368, 0x44468, 0x47278, 0x45378, 0x47288, 0x44388, 0x4834a, 0x4744a, 0x4735a, 0x4545a, 0x4455a, 0x4826a, 0x4636a, 0x4446a, 0x4727a, 0x4537a, 0x4728a, 0x4438a, 0x6d346, 0x6b446, 0x6e256, 0x6c356, 0x69456, 0x66556, 0x6d266, 0x6a366, 0x66466, 0x6c276, 0x68376, 0x64476,
	0x6b286, 0x66386, 0x67348, 0x66448, 0x68258, 0x66358, 0x64458, 0x67268, 0x65368, 0x67278, 0x64378, 0x66288, 0x6734a, 0x6644a, 0x6825a, 0x6635a, 0x6445a, 0x6726a, 0x6536a, 0x6727a, 0x6437a, 0x6628a, 0x8e336, 0x8c346, 0x8a446, 0x8e256, 0x8b356, 0x87456, 0x84556, 0x8c266, 0x89366, 0x85466,
	0x8b276, 0x87376, 0x8a286, 0x85386, 0x88338, 0x87348, 0x85448, 0x88258, 0x86358, 0x84458, 0x87268, 0x84368, 0x86278, 0x85288, 0x8833a, 0x8734a, 0x8544a, 0x8825a, 0x8635a, 0x8445a, 0x8726a, 0x8436a, 0x8627a, 0x8528a, 0xae336, 0xae246, 0xab346, 0xa8446, 0xad256, 0xa9356, 0xa5456, 0xab266,
	0xa7366, 0xaa276, 0xa5376, 0xa8286, 0xa8338, 0xa8248, 0xa6348, 0xa4448, 0xa7258, 0xa5358, 0xa6268, 0xa5278, 0xa4288, 0xa833a, 0xa824a, 0xa634a, 0xa444a, 0xa725a, 0xa535a, 0xa626a, 0xa527a, 0xa428a, 0xcd336, 0xce246, 0xca346, 0xc7446, 0xcc256, 0xc8356, 0xc4456, 0xca266, 0xc5366, 0xc9276,
	0xc7286, 0xc7338, 0xc8248, 0xc5348, 0xc7258, 0xc4358, 0xc5268, 0xc4278, 0xc733a, 0xc824a, 0xc534a, 0xc725a, 0xc435a, 0xc526a, 0xc427a, 0xec336, 0xed246, 0xe9346, 0xe6446, 0xeb256, 0xe7356, 0xe9266, 0xe4366, 0xe8276, 0xe6286, 0xe7338, 0xe7248, 0xe5348, 0xe6258, 0xe5268, 0xe4278, 0xe733a,
	0xe724a, 0xe534a, 0xe625a, 0xe526a, 0xe427a, 0x10e236, 0x10b336, 0x10c246, 0x108346, 0x104446, 0x10a256, 0x105356, 0x108266, 0x106276, 0x104286, 0x108238, 0x106338, 0x107248, 0x104348, 0x105258, 0x104268, 0x10823a, 0x10633a, 0x10724a, 0x10434a, 0x10525a, 0x10426a, 0x12e236, 0x12b336, 0x12c246, 0x127346, 0x129256,
	0x124356, 0x127266, 0x125276, 0x128238, 0x126338, 0x126248, 0x124348, 0x125258, 0x124268, 0x12823a, 0x12633a, 0x12624a, 0x12434a, 0x12525a, 0x12426a, 0x14d236, 0x14a336, 0x14b246, 0x146346, 0x149256, 0x146266, 0x144276, 0x147238, 0x145338, 0x146248, 0x144258, 0x14723a, 0x14533a, 0x14624a, 0x14425a, 0x16d236, 0x169336,
	0x16a246, 0x165346, 0x168256, 0x165266, 0x167238, 0x165338, 0x165248, 0x164258, 0x16723a, 0x16533a, 0x16524a, 0x16425a
};

bool init_multi_subset_context(
	subset_enc_context& ctx,
	uint32_t max_subsets,
	uint32_t num_carrier_candidates, uint32_t num_pattern_candidates, 
	float two_subset_var_thresh, uint32_t two_subset_dot_thresh_fract_index, 
	float three_subset_var_thresh,
	astc_ldr::partitions_data* pPart_data_p2, astc_ldr::partitions_data* pPart_data_p3)
{
	assert((max_subsets >= 1) && (max_subsets <= 3));

	assert(pPart_data_p2 && pPart_data_p2->m_part_lhs_map.is_valid());
	assert(pPart_data_p3 && pPart_data_p3->m_part_lhs_map.is_valid());

	assert(astc_helpers::is_valid_block_size(ctx.m_block_width, ctx.m_block_height) && 
		(astc_helpers::find_astc_block_size_index(ctx.m_block_width, ctx.m_block_height) == (int)ctx.m_block_size_index));
	
	assert(ctx.m_total_block_pixels == (ctx.m_block_width * ctx.m_block_height));
		
	ctx.m_max_subsets = max_subsets;
	ctx.m_num_carrier_candidates = num_carrier_candidates;
	ctx.m_num_pattern_candidates = num_pattern_candidates;
	ctx.m_two_subset_var_thresh = two_subset_var_thresh;
	ctx.m_three_subset_var_thresh = three_subset_var_thresh;
	ctx.m_two_subset_dot_thresh_fract_index = two_subset_dot_thresh_fract_index;

	const uint32_t num_two_subset_unique_pats = basist::astc_ldr_t::get_total_unique_patterns(ctx.m_block_size_index, 2);

	ctx.m_num_unique_two_subset_pats = num_two_subset_unique_pats;
	ctx.m_pUnique_two_subset_pats = basist::astc_ldr_t::g_unique_index_to_astc_part_seed[0][ctx.m_block_size_index];

	ctx.m_use_method1 = true;
	ctx.m_use_method2 = true;

	ctx.m_pPart_data_p2 = pPart_data_p2;
	ctx.m_pPart_data_p3 = pPart_data_p3;
	
	for (uint32_t unique_index = 0; unique_index < num_two_subset_unique_pats; unique_index++)
	{
		const uint32_t seed_id = ctx.m_pUnique_two_subset_pats[unique_index];
		assert(seed_id == basist::astc_ldr_t::unique_pat_index_to_part_seed(ctx.m_block_size_index, 2, unique_index));
		
		bitmask192 pat_bitmask(0, 0, 0);
		uint32_t bit_ofs = 0;

		for (uint32_t y = 0; y < ctx.m_block_height; y++)
		{
			for (uint32_t x = 0; x < ctx.m_block_width; x++)
			{
				uint64_t s = astc_helpers::get_precomputed_texel_partition(ctx.m_block_width, ctx.m_block_height, seed_id, x, y, 2);
				
				if (s)
				{
					bitmask192 b(0, 0, 0);
					b.set_bit(bit_ofs);

					pat_bitmask |= b;
				}

				++bit_ofs;
			} // x
		} // y

		ctx.m_two_subset_pat_bitmask[unique_index] = pat_bitmask;

	} // unique_index

	return true;
}

// gradient descent: refines weights given current 2 subset endpoints
// returns true if weight grid changed
static bool weight_gradient_descent(
	const pixelbuf& src_block,
	astc_helpers::log_astc_block& lblock_to_refine,
	const pixelbuf subset_pbuf[3], uint32_t subset_pixel_indices[3][256],
	const astc_unpacked_config& best_config,
	const subset_enc_context& enc_state,
	const basist::astc_ldr_t::astc_block_grid_data* pGrid_data,
	double& best_lblock_error, astc_helpers::log_astc_block& best_lblock,
	astc_lblock_vec* pAll_candidates)
{
	assert(!is_lblock_ise(lblock_to_refine));
	assert((lblock_to_refine.m_num_partitions >= 2) && (lblock_to_refine.m_num_partitions <= 3));
		
	const uint32_t block_width = enc_state.m_block_width, block_height = enc_state.m_block_height;
	const uint32_t total_block_pixels = block_width * block_height;

	const uint32_t num_cem_endpoint_vals = astc_helpers::get_num_cem_values(best_config.m_cem);
	const uint32_t total_grid_weights = best_config.m_grid_width * best_config.m_grid_height;

	[[maybe_unused]] const auto& endpoint_tab = astc_helpers::g_dequant_tables.get_endpoint_tab(best_config.m_endpoint_range);
	const auto& weight_tab = astc_helpers::g_dequant_tables.get_weight_tab(best_config.m_weight_range);

	[[maybe_unused]] const uint32_t num_weight_levels = astc_helpers::get_ise_levels(best_config.m_weight_range);

	const uint32_t num_partitions = lblock_to_refine.m_num_partitions;

	// compute the weights we want (the "ideal" weights) minus the upsampled weights weights we have, then project that residual back to weight grid res

	// compute ideal [0,64] block weights given the current endpoints
	uint8_t ideal_block_weight_vals[astc_helpers::MAX_BLOCK_PIXELS]; // at block res, ranks, [0,64] dequantized values

#if defined(DEBUG) || defined(_DEBUG)            
	memset(ideal_block_weight_vals, 0xFF, astc_helpers::MAX_BLOCK_PIXELS);
#endif

	for (uint32_t s = 0; s < num_partitions; s++)
	{
		uint8_t weights[astc_helpers::MAX_BLOCK_PIXELS];

		eval_weights_first_plane(subset_pbuf[s], 
			weights, astc_helpers::BISE_64_LEVELS, 
			best_config.m_cem, lblock_to_refine.m_endpoints + s * num_cem_endpoint_vals, best_config.m_endpoint_range, 
			get_num_cem_chans(best_config.m_cem));

		for (uint32_t j = 0; j < subset_pbuf[s].m_width; j++)
		{
			const uint32_t pixel_index = subset_pixel_indices[s][j];
			assert(pixel_index < total_block_pixels);

			ideal_block_weight_vals[pixel_index] = weights[j]; // [0,64] weight value space, or in rank space
		} // j

	} // s

#if defined(DEBUG) || defined(_DEBUG)
	for (uint32_t i = 0; i < total_block_pixels; i++)
	{
		assert(ideal_block_weight_vals[i] != 0xFF);
	}
#endif

	// upsample the current weight grid

	uint8_t dequant_grid_weights[astc_helpers::MAX_GRID_WEIGHTS]; // grid res, [0,64]

	for (uint32_t i = 0; i < total_grid_weights; i++)
		dequant_grid_weights[i] = (uint8_t)weight_tab.get_rank_to_val(lblock_to_refine.m_weights[i]);

	uint8_t dequant_block_weights[astc_helpers::MAX_BLOCK_PIXELS]; // block res, [0,64]
	if ((lblock_to_refine.m_grid_width == (int)block_width) && (lblock_to_refine.m_grid_height == (int)block_height))
		memcpy(dequant_block_weights, dequant_grid_weights, total_block_pixels);
	else
		astc_helpers::upsample_weight_grid_xuastc_ldr(block_width, block_height, lblock_to_refine.m_grid_width, lblock_to_refine.m_grid_height, dequant_grid_weights, dequant_block_weights, nullptr, nullptr);

	// now compute the residual at block res

	int weight_block_raw_residuals[astc_helpers::MAX_BLOCK_PIXELS]; // block res, [0,64]

	for (uint32_t i = 0; i < total_block_pixels; i++)
		weight_block_raw_residuals[i] = ideal_block_weight_vals[i] - dequant_block_weights[i];

	// downsample the residuals to grid res

	const basisu::vector<float>& unweighted_downsample_matrix = pGrid_data->m_unweighted_downsample_matrix;
	const basisu::vector<float>& one_over_diag_AtA = pGrid_data->m_one_over_diag_AtA;

	float weight_grid_residuals_downsampled[astc_helpers::MAX_GRID_WEIGHTS]; // grid res, [0,64]

	basisu::astc_ldr::downsample_weight_residual_grid(
		unweighted_downsample_matrix.get_ptr(),
		block_width, block_height,		// source/from dimension (block size)
		best_config.m_grid_width, best_config.m_grid_height,		// dest/to dimension (grid size)
		weight_block_raw_residuals,	// these are dequantized weights, NOT ISE symbols, [by][bx]
		weight_grid_residuals_downsampled);			// [wy][wx]

	for (uint32_t i = 0; i < total_grid_weights; i++)
		weight_grid_residuals_downsampled[i] *= one_over_diag_AtA[i];

	// Apply the residuals at grid res and quantize
	const float Q = 1.0f;

	astc_helpers::log_astc_block refined_lblock(lblock_to_refine);

	bool changed_flag = false;
	
	for (uint32_t i = 0; i < total_grid_weights; i++)
	{
		float v = (float)weight_tab.get_rank_to_val(lblock_to_refine.m_weights[i]) + weight_grid_residuals_downsampled[i] * Q;

		const int iv = clamp((int)std::roundf(v), 0, 64);

		uint8_t new_weight = (uint8_t)weight_tab.get_val_to_rank(iv);

		if (refined_lblock.m_weights[i] != new_weight)
		{
			refined_lblock.m_weights[i] = new_weight;
			changed_flag = true;
		}
	}

	if (changed_flag)
	{
		const double refined_error = compute_block_error(refined_lblock, src_block, enc_state);

		if (refined_error < best_lblock_error)
		{
			best_lblock_error = refined_error;
			memcpy(&best_lblock, &refined_lblock, sizeof(best_lblock));
		}
		
		if (pAll_candidates)
			pAll_candidates->push_back(refined_lblock);

		lblock_to_refine = refined_lblock;
	}

	return changed_flag;
}

// given current weight grid, refine subset endpoints
// returns true if improved
static bool refine_endpoints_with_current_weights(
	const pixelbuf &src_block,
	astc_helpers::log_astc_block& lblock_to_refine,
	const pixelbuf subset_pbuf[3], uint32_t subset_pixel_indices[3][256],
	const astc_unpacked_config& best_config,
	const subset_enc_context& enc_state,
	double& best_lblock_error, astc_helpers::log_astc_block& best_lblock,
	astc_lblock_vec* pAll_candidates)
{
	assert(!is_lblock_ise(lblock_to_refine));
	assert((lblock_to_refine.m_num_partitions >= 2) && (lblock_to_refine.m_num_partitions <= 3));

	const uint32_t block_width = enc_state.m_block_width, block_height = enc_state.m_block_height;
	const uint32_t total_block_pixels = block_width * block_height;

	const uint32_t num_cem_endpoint_vals = astc_helpers::get_num_cem_values(best_config.m_cem);
	//const uint32_t num_weight_levels = get_levels(best_config.m_weight_range);

	const uint32_t total_grid_weights = best_config.m_grid_width * best_config.m_grid_height;

	const uint32_t num_partitions = lblock_to_refine.m_num_partitions;

	[[maybe_unused]] const auto& endpoint_tab = astc_helpers::g_dequant_tables.get_endpoint_tab(best_config.m_endpoint_range);
	const auto& weight_tab = astc_helpers::g_dequant_tables.get_weight_tab(best_config.m_weight_range);

	astc_helpers::log_astc_block trial_block(lblock_to_refine);

	uint8_t dequant_grid_weights[astc_helpers::MAX_GRID_WEIGHTS]; // grid res, [0,64]

	for (uint32_t i = 0; i < total_grid_weights; i++)
	{
		dequant_grid_weights[i] = (uint8_t)weight_tab.get_rank_to_val(trial_block.m_weights[i]);
	}

	uint8_t dequant_block_weights[astc_helpers::MAX_BLOCK_PIXELS]; // block res, [0,64]

	if ((trial_block.m_grid_width == (int)block_width) && (trial_block.m_grid_height == (int)block_height))
		memcpy(dequant_block_weights, dequant_grid_weights, total_block_pixels);
	else
		astc_helpers::upsample_weight_grid_xuastc_ldr(block_width, block_height, trial_block.m_grid_width, trial_block.m_grid_height, dequant_grid_weights, dequant_block_weights, nullptr, nullptr);

	const uint32_t num_cem_chans = get_num_cem_chans(best_config.m_cem);

	for (uint32_t subset_index = 0; subset_index < num_partitions; subset_index++)
	{
		const uint32_t num_subset_pixels = subset_pbuf[subset_index].m_width;
		assert(num_subset_pixels);

		uint8_t subset_weights[astc_helpers::MAX_BLOCK_PIXELS];
		for (uint32_t i = 0; i < num_subset_pixels; i++)
		{
			const uint32_t subset_pixel_index = subset_pixel_indices[subset_index][i];

			subset_weights[i] = dequant_block_weights[subset_pixel_index];
		} // i

		float new_cem_valsf[8];
		
		refine_endpoints_given_weights(subset_pbuf[subset_index], best_config.m_cem, subset_weights, astc_helpers::BISE_64_LEVELS,
			lblock_to_refine.m_endpoints + num_cem_endpoint_vals * subset_index, best_config.m_endpoint_range, new_cem_valsf, num_cem_chans);

		uint8_t new_cem_vals[8];
		cem_encode(best_config.m_cem, new_cem_valsf, best_config.m_endpoint_range, new_cem_vals, true, enc_state.m_higher_effort_bc);

		for (uint32_t i = 0; i < num_cem_endpoint_vals; i++)
			trial_block.m_endpoints[num_cem_endpoint_vals * subset_index + i] = new_cem_vals[i];

	} // subset_index

	const double trial_error = compute_block_error(trial_block, src_block, enc_state);
	bool status = false;

	if (trial_error < best_lblock_error)
	{
		best_lblock_error = trial_error;
		memcpy(&best_lblock, &trial_block, sizeof(best_lblock));
		status = true;
	}

	if (pAll_candidates)
		pAll_candidates->push_back(trial_block);

	lblock_to_refine = trial_block;

	return status;
}

static void compress_block_2subsets_internal(
	const subset_enc_context& enc_context,
	const uint8_t* pBlock_pixels, 
	double& best_lblock_error, astc_helpers::log_astc_block& best_lblock,
	astc_lblock_vec* pAll_candidates,
	single_subset_shortlist_state &shortlist_state)
{
	assert(enc_context.m_use_method1 || enc_context.m_use_method2);
		
	const uint32_t block_width = enc_context.m_block_width, block_height = enc_context.m_block_height;
	const uint32_t total_block_pixels = enc_context.m_total_block_pixels;

	const uint32_t num_src_block_chans = shortlist_state.m_num_src_block_comps;
	[[maybe_unused]] const bool has_a = (num_src_block_chans == 4);

	rgba32_image block_img;
	block_img.m_pPixels = pBlock_pixels;
	block_img.m_width = block_width;
	block_img.m_height = block_height;
	block_img.m_row_pitch_in_texels = block_width;

	const float SUBSET_SCALE_WEIGHT = 2.0f;

	const uint32_t total_candidates = generate_single_subset_shortlist(
		TOTAL_TWO_SUBSET_CONFIGS_RGBA, g_two_subset_configs_rgba,
		enc_context,
		block_img,
		shortlist_state.m_src_is_luma_only,
		shortlist_state.m_num_src_block_comps,
		shortlist_state, SUBSET_SCALE_WEIGHT, enc_context.m_num_carrier_candidates);

	if (!total_candidates)
	{
		assert(0);
		return;
	}
		
#if 0
	encode_single_subset_block(enc_context, shortlist_state.m_pbuf, shortlist_state.m_num_src_block_comps, shortlist_state.m_best_configs[0], best_lblock, false);

	best_lblock.m_color_endpoint_modes[1] = best_lblock.m_color_endpoint_modes[0];
	
	uint32_t num_endpoint_vals = astc_helpers::get_num_cem_values(best_lblock.m_color_endpoint_modes[0]);
	memcpy(best_lblock.m_endpoints + num_endpoint_vals, best_lblock.m_endpoints, num_endpoint_vals);

	best_lblock.m_num_partitions = 2;
	best_lblock.m_partition_id = 1;

	convert_rank_lblock_to_ise(best_lblock);
	return;
#endif
		
	static const uint8_t s_num_dot_thresh_fracs[NUM_DOT_THRESH_FRACTS] = { 1, 3, 5, 9, 11, 15 };

	static const float s_dot_thresh_fracs15[15] = {	-.55f, -0.45f, -0.35f, -0.25f, -0.15f, -0.10f, -0.075f, 0.0f, 0.075f, 0.10f, 0.15f, 0.25f, 0.35f, 0.45f, 0.55f };
	static const float s_dot_thresh_fracs11[11] = { -0.45f, -0.35f, -0.25f, -0.15f, -0.075f, 0.0f, 0.075f, 0.15f, 0.25f, 0.35f, 0.45f };
	static const float s_dot_thresh_fracs9[9] = { -0.35f, -0.25f, -0.15f, -0.075f, 0.0f, 0.075f, 0.15f, 0.25f, 0.35f };
	static const float s_dot_thresh_fracs5[5] = { -.2f, -0.1f, 0.0f, .1f, .2f };
	static const float s_dot_thresh_fracs3[3] = { -0.1f, 0.0f, .1f };
	static const float s_dot_thresh_fracs1[1] = { 0.0f };
	static const float* s_pDot_thresh_fracts[NUM_DOT_THRESH_FRACTS] = { s_dot_thresh_fracs1, s_dot_thresh_fracs3, s_dot_thresh_fracs5, s_dot_thresh_fracs9, s_dot_thresh_fracs11, s_dot_thresh_fracs15 };
		
	const uint32_t dt_index = minimum<uint32_t>(NUM_DOT_THRESH_FRACTS - 1, enc_context.m_two_subset_dot_thresh_fract_index);
	const uint32_t num_dot_thresh_fracts = s_num_dot_thresh_fracs[dt_index];
	const float* const pDot_thresh_fracts = s_pDot_thresh_fracts[dt_index];
		
	float dot_range = 0;

	if (num_dot_thresh_fracts > 1)
	{
		const block_stats& stats = shortlist_state.m_stats;

		const float mean_r = stats.m_mean[0], mean_g = stats.m_mean[1], mean_b = stats.m_mean[2], mean_a = stats.m_mean[3];
		const float axis_r = stats.m_axis[0], axis_g = stats.m_axis[1], axis_b = stats.m_axis[2], axis_a = stats.m_axis[3];

		float min_dot = FLT_MAX;
		float max_dot = -FLT_MAX;

		const uint8_t* pSrc_pixels = pBlock_pixels;

		for (uint32_t y = 0; y < block_height; y++)
		{
			for (uint32_t x = 0; x < block_width; x++)
			{
				const float r = (float)pSrc_pixels[0] - mean_r;
				const float g = (float)pSrc_pixels[1] - mean_g;
				const float b = (float)pSrc_pixels[2] - mean_b;
				const float a = (float)pSrc_pixels[3] - mean_a;
				pSrc_pixels += 4;

				const float dot = r * axis_r + g * axis_g + b * axis_b + a * axis_a;

				min_dot = minimum(min_dot, dot);
				max_dot = maximum(max_dot, dot);
			}
		}
		
		dot_range = max_dot - min_dot;
	}

	for (uint32_t config_cand_index = 0; config_cand_index < total_candidates; config_cand_index++)
	{
		const astc_unpacked_config& best_config = shortlist_state.m_best_configs[config_cand_index];

		const bool cem_has_a = does_cem_have_alpha(best_config.m_cem);
		const uint32_t num_cem_comps = get_num_cem_chans(best_config.m_cem);

		const basist::astc_ldr_t::astc_block_grid_data* pGrid_data = basist::astc_ldr_t::find_astc_block_grid_data(block_width, block_height, best_config.m_grid_width, best_config.m_grid_height);

		astc_helpers::log_astc_block carrier_lblock; // carrier block in rank space

		encode_single_subset_block(enc_context, shortlist_state.m_pbuf, shortlist_state.m_num_src_block_comps, best_config, carrier_lblock, false);

		// base+ofs is so marginal that it's not worth the effort/extra complexity here
		assert(!is_cem_9_or_13(carrier_lblock.m_color_endpoint_modes[0]));

		convert_ise_lblock_to_rank(carrier_lblock);

		for (uint32_t dot_thresh_fract_iter = 0; dot_thresh_fract_iter < num_dot_thresh_fracts; dot_thresh_fract_iter++)
		{
			const float dot_thresh_fract = pDot_thresh_fracts[dot_thresh_fract_iter];

			bitmask192 desired_bitmask(0, 0, 0);

			{
				const block_stats& stats = shortlist_state.m_stats;

				const float mean_r = stats.m_mean[0], mean_g = stats.m_mean[1], mean_b = stats.m_mean[2], mean_a = stats.m_mean[3];
				const float axis_r = stats.m_axis[0], axis_g = stats.m_axis[1], axis_b = stats.m_axis[2], axis_a = stats.m_axis[3];

				uint32_t bit_ofs = 0;
				const uint8_t* pSrc_pixels = pBlock_pixels;

				const float subset_dot_thresh = dot_thresh_fract * dot_range;

				for (uint32_t y = 0; y < block_height; y++)
				{
					for (uint32_t x = 0; x < block_width; x++)
					{
						const float r = ((float)pSrc_pixels[0] - mean_r);
						const float g = ((float)pSrc_pixels[1] - mean_g);
						const float b = ((float)pSrc_pixels[2] - mean_b);
						const float a = ((float)pSrc_pixels[3] - mean_a);
						pSrc_pixels += 4;

						const float dot = (r * axis_r) + (g * axis_g) + (b * axis_b) + (a * axis_a);

						const uint32_t s = (dot > subset_dot_thresh) ? 1 : 0;
						desired_bitmask.set_bit(bit_ofs, s);

						bit_ofs++;
					} // x
				} // y
			}

			const bitmask192 active_bitmask(bitmask192::lsb_mask(total_block_pixels));
			const bitmask192 desired_bitmask_inverted = (~desired_bitmask) & active_bitmask;

			assert(desired_bitmask <= active_bitmask);
			assert(desired_bitmask_inverted <= active_bitmask);

			uint32_t best_diffs[MAX_UNIQUE_2SUBSET_PATS];
			assert(enc_context.m_num_unique_two_subset_pats <= MAX_UNIQUE_2SUBSET_PATS);

			if (enc_context.m_num_pattern_candidates == 1)
			{
				uint32_t best_diff = UINT32_MAX, best_diff_inverted = UINT32_MAX;

				for (uint32_t i = 0; i < enc_context.m_num_unique_two_subset_pats; i++)
				{
					const bitmask192 pat_bitmask = enc_context.m_two_subset_pat_bitmask[i];
			
					const uint32_t diff = (popcount192(pat_bitmask ^ desired_bitmask) << 16) | i;
					const uint32_t diff_inverted = (popcount192(pat_bitmask ^ desired_bitmask_inverted) << 16) | i;

					best_diff = minimum(diff, best_diff);
					best_diff_inverted = minimum(diff_inverted, best_diff_inverted);
				}

				best_diffs[0] = minimum(best_diff, best_diff_inverted);
			}
			else
			{
				for (uint32_t i = 0; i < enc_context.m_num_unique_two_subset_pats; i++)
				{
					const bitmask192 pat_bitmask = enc_context.m_two_subset_pat_bitmask[i];
					assert(pat_bitmask <= active_bitmask);

					const uint32_t diff = popcount192(pat_bitmask ^ desired_bitmask);
					const uint32_t diff_inverted = popcount192(pat_bitmask ^ desired_bitmask_inverted);

					best_diffs[i] = (minimum(diff, diff_inverted) << 16) + i;
				}

				std::sort(best_diffs, best_diffs + enc_context.m_num_unique_two_subset_pats);
			}

			for (uint32_t pat_cand_iter = 0; pat_cand_iter < enc_context.m_num_pattern_candidates; pat_cand_iter++)
			{
				const uint32_t best_unique_pat_index = best_diffs[pat_cand_iter] & 1023;
				assert(best_unique_pat_index < enc_context.m_num_unique_two_subset_pats);

				const bitmask192 best_pat_bitmask = enc_context.m_two_subset_pat_bitmask[best_unique_pat_index];

				const uint32_t best_seed_id = enc_context.m_pUnique_two_subset_pats[best_unique_pat_index];

				// [3] but we only use [2] here

				float subset_pixels[3][PIXELBUF_SIZE_IN_FLOATS];

				pixelbuf subset_pbuf[3];
				subset_pbuf[0].m_pBuf = subset_pixels[0];
				subset_pbuf[0].m_width = 0;
				subset_pbuf[0].m_height = 1;

				subset_pbuf[1].m_pBuf = subset_pixels[1];
				subset_pbuf[1].m_width = 0;
				subset_pbuf[1].m_height = 1;

				uint32_t subset_pixel_indices[3][256];

				const uint8_t* pSrc_pixels = pBlock_pixels;
				uint32_t bit_ofs = 0;
						
				for (uint32_t y = 0; y < block_height; y++)
				{
					for (uint32_t x = 0; x < block_width; x++)
					{
						uint32_t s = (uint32_t)(best_pat_bitmask.is_bit_set(bit_ofs));
						bit_ofs++;

						const float r = (float)pSrc_pixels[0];
						const float g = (float)pSrc_pixels[1];
						const float b = (float)pSrc_pixels[2];
						const float a = cem_has_a ? (float)pSrc_pixels[3] : 255;
						pSrc_pixels += 4;

						const uint32_t cur_width = subset_pbuf[s].m_width;

						subset_pixel_indices[s][cur_width] = x + y * block_width;

						pixelbuf_set_comp(subset_pbuf[s], cur_width, 0, 0, r);
						pixelbuf_set_comp(subset_pbuf[s], cur_width, 0, 1, g);
						pixelbuf_set_comp(subset_pbuf[s], cur_width, 0, 2, b);
						pixelbuf_set_comp(subset_pbuf[s], cur_width, 0, 3, a);

						subset_pbuf[s].m_width = cur_width + 1;

					} // x

				} // y

				assert(subset_pbuf[0].m_width && subset_pbuf[1].m_width);

				// -------

				astc_helpers::log_astc_block final_lblock;
				final_lblock.clear();
				final_lblock.m_user_mode = cUserModeRankValues;

				final_lblock.m_grid_width = carrier_lblock.m_grid_width;
				final_lblock.m_grid_height = carrier_lblock.m_grid_height;
				final_lblock.m_endpoint_ise_range = best_config.m_endpoint_range;
				final_lblock.m_weight_ise_range = best_config.m_weight_range;
				final_lblock.m_num_partitions = 2;
				final_lblock.m_partition_id = safe_cast_uint16(best_seed_id);
				final_lblock.m_color_endpoint_modes[0] = best_config.m_cem;
				final_lblock.m_color_endpoint_modes[1] = best_config.m_cem;

				// -------

				const uint32_t num_cem_endpoint_vals = astc_helpers::get_num_cem_values(best_config.m_cem);
				const uint32_t total_grid_weights = carrier_lblock.m_grid_width * carrier_lblock.m_grid_height;

				//const auto& endpoint_tab = astc_helpers::g_dequant_tables.get_endpoint_tab(best_config.m_endpoint_range);
				const auto& weight_tab = astc_helpers::g_dequant_tables.get_weight_tab(best_config.m_weight_range);

				[[maybe_unused]] const uint32_t num_weight_levels = astc_helpers::get_ise_levels(best_config.m_weight_range);

				// ------- method 1
				if (enc_context.m_use_method1)
				{
					memcpy(final_lblock.m_weights, carrier_lblock.m_weights, total_grid_weights);
					memcpy(final_lblock.m_endpoints, carrier_lblock.m_endpoints, num_cem_endpoint_vals);
					memcpy(final_lblock.m_endpoints + num_cem_endpoint_vals, carrier_lblock.m_endpoints, num_cem_endpoint_vals);

					const uint32_t NUM_M1_PASSES = 2;

					for (uint32_t pass = 0; pass < NUM_M1_PASSES; pass++)
					{
						refine_endpoints_with_current_weights(shortlist_state.m_pbuf, final_lblock, subset_pbuf, subset_pixel_indices, best_config, enc_context,
							best_lblock_error, best_lblock, nullptr);

						bool changed_flag;

						if (pass == (NUM_M1_PASSES - 1))
						{
							changed_flag = weight_polish(shortlist_state.m_pbuf, final_lblock, enc_context, pGrid_data,
								best_lblock_error, best_lblock, nullptr);
						}
						else
						{
							changed_flag = weight_gradient_descent(shortlist_state.m_pbuf, final_lblock, subset_pbuf, subset_pixel_indices, best_config, enc_context, pGrid_data,
								best_lblock_error, best_lblock, nullptr);
						}

						if (!changed_flag)
						{
							break;
						}
					}

					if (pAll_candidates)
					{
						pAll_candidates->push_back(final_lblock);
					}
				}

				double best_pat_lblock_error = DBL_MAX;
				astc_helpers::log_astc_block best_pat_lblock;

				// ------- method 2
				if (enc_context.m_use_method2)
				{
					uint8_t desired_block_weights[astc_helpers::MAX_BLOCK_PIXELS]; // at block res, ranks

#if defined(DEBUG) || defined(_DEBUG)            
					memset(desired_block_weights, 0xFF, astc_helpers::MAX_BLOCK_PIXELS);
#endif					
					for (uint32_t s = 0; s < 2; s++)
					{
						float initial_endpoints[8];
						calc_initial_cem_endpoints(subset_pbuf[s], initial_endpoints, num_cem_comps, nullptr, is_cem_6_or_10(best_config.m_cem));

						uint8_t* pSubset_CEM_vals = final_lblock.m_endpoints + s * num_cem_endpoint_vals;
						cem_encode(best_config.m_cem, initial_endpoints, best_config.m_endpoint_range, pSubset_CEM_vals, true, enc_context.m_higher_effort_bc);

						uint8_t subset_weights[astc_helpers::MAX_BLOCK_PIXELS];
						eval_weights_first_plane(subset_pbuf[s], subset_weights, best_config.m_weight_range,
							best_config.m_cem, pSubset_CEM_vals, best_config.m_endpoint_range,
							num_cem_comps);

						for (uint32_t i = 0; i < enc_context.m_num_ls_iterations; i++)
						{
							float refined_endpoints[8];
							refine_endpoints_given_weights(subset_pbuf[s], best_config.m_cem, 
								subset_weights, best_config.m_weight_range, pSubset_CEM_vals, best_config.m_endpoint_range, refined_endpoints, num_cem_comps);
														
							cem_encode(best_config.m_cem, refined_endpoints, best_config.m_endpoint_range, pSubset_CEM_vals, true, enc_context.m_higher_effort_bc);

							eval_weights_first_plane(subset_pbuf[s], subset_weights, best_config.m_weight_range,
								best_config.m_cem, pSubset_CEM_vals, best_config.m_endpoint_range,
								num_cem_comps);
						} // i

						// base+ofs is so rarely a win (< ~1%) that it's not worth the trouble

						for (uint32_t j = 0; j < subset_pbuf[s].m_width; j++)
						{
							const uint32_t pixel_index = subset_pixel_indices[s][j];
							assert(pixel_index < total_block_pixels);

							desired_block_weights[pixel_index] = subset_weights[j];
						}

					} // s

#if defined(DEBUG) || defined(_DEBUG)
					for (uint32_t i = 0; i < total_block_pixels; i++)
					{
						assert(desired_block_weights[i] != 0xFF);
					}
#endif

					// now downsample the ideal weights to grid res, quantize
					// desired_block_weights[] is either [0,64] (dequantized weight values) or in the carrier's weight rank space depending on solve_weights_dequantized

					if ((block_width == best_config.m_grid_width) && (block_height == best_config.m_grid_height))
					{
						assert(total_block_pixels == total_grid_weights);

memcpy(final_lblock.m_weights, desired_block_weights, total_block_pixels);
					}
					else
					{
						// dequantize ranks to [0,64] values
						for (uint32_t i = 0; i < total_block_pixels; i++)
							desired_block_weights[i] = (uint8_t)weight_tab.get_rank_to_val(desired_block_weights[i]);

						// downsample from block to grid res, [0,64] values
						uint8_t downsampled_weights[astc_helpers::MAX_GRID_WEIGHTS];

#if 0
						basisu::downsample_weight_grid(pGrid_data->m_downsample_matrix.get_ptr(),
							block_width, block_height,
							best_config.m_grid_width, best_config.m_grid_height,
							desired_block_weights,
							downsampled_weights);
#else
						float src_temp[PIXELBUF_COMP_PITCH];
						pixelbuf src_pbuf(block_width, block_height, src_temp);
						for (uint32_t y = 0; y < block_height; y++)
							for (uint32_t x = 0; x < block_width; x++)
								pixelbuf_set_comp(src_pbuf, x, y, 0, desired_block_weights[x + y * block_width]);

						float dst_temp[PIXELBUF_COMP_PITCH];
						pixelbuf dst_pbuf(best_config.m_grid_width, best_config.m_grid_height, dst_temp);

						pseudoinverse_block_to_grid(src_pbuf, dst_pbuf, 1);
						for (uint32_t y = 0; y < best_config.m_grid_height; y++)
							for (uint32_t x = 0; x < best_config.m_grid_width; x++)
								downsampled_weights[x + y * best_config.m_grid_width] = (uint8_t)clamp((int)std::round(pixelbuf_get_comp(dst_pbuf, x, y, 0)), 0, 64);
#endif

						// quantize downsampled weights
						for (uint32_t i = 0; i < total_grid_weights; i++)
							final_lblock.m_weights[i] = (uint8_t)weight_tab.get_val_to_rank(downsampled_weights[i]);
					}

					{
						// convert from rank to ISE space
						const double subset_error = compute_block_error(final_lblock, shortlist_state.m_pbuf, enc_context);

						if (subset_error < best_pat_lblock_error)
						{
							best_pat_lblock_error = subset_error;
							best_pat_lblock = final_lblock;
						}
					}

					const uint32_t NUM_M2_PASSES = 2;

					for (uint32_t pass = 0; pass < NUM_M2_PASSES; pass++)
					{
						refine_endpoints_with_current_weights(shortlist_state.m_pbuf, final_lblock, subset_pbuf, subset_pixel_indices, best_config, enc_context,
							best_pat_lblock_error, best_pat_lblock, nullptr);

						bool changed_flag;

						if (pass == (NUM_M2_PASSES - 1))
						{
							changed_flag = weight_polish(shortlist_state.m_pbuf, final_lblock, enc_context, pGrid_data,
								best_pat_lblock_error, best_pat_lblock, nullptr);
						}
						else
						{
							changed_flag = weight_gradient_descent(shortlist_state.m_pbuf, final_lblock, subset_pbuf, subset_pixel_indices, best_config, enc_context, pGrid_data,
								best_pat_lblock_error, best_pat_lblock, nullptr);
						}

						if (!changed_flag)
						{
							break;
						}
					}

					assert(best_pat_lblock_error != DBL_MAX);
					if (best_pat_lblock_error < best_lblock_error)
					{
						best_lblock_error = best_pat_lblock_error;
						best_lblock = best_pat_lblock;
					}

					if (pAll_candidates)
					{
						pAll_candidates->push_back(best_pat_lblock);
					}
				}

			} // pat_cand_iter

		} // ds

	} // config_cand_index

	convert_rank_lblock_to_ise(best_lblock);
}

static bool create_desired_partitions_3subsets(
	const block_stats& stats,
	uint32_t total_block_pixels,
	const uint8_t *pBlock_pixels,
	uint8_t *pDesired_part)
{
	memset(pDesired_part, 0, total_block_pixels);

	const uint32_t NUM_SUBSETS = 3;

	const float mean_r = stats.m_mean[0], mean_g = stats.m_mean[1], mean_b = stats.m_mean[2], mean_a = stats.m_mean[3];
	const float axis_r = stats.m_axis[0], axis_g = stats.m_axis[1], axis_b = stats.m_axis[2], axis_a = stats.m_axis[3];

	float cluster_centroids[NUM_SUBSETS][4];
	clear_obj(cluster_centroids);

	float brightest_inten = -BIG_FLOAT_VAL, darkest_inten = BIG_FLOAT_VAL;

	const uint8_t* pSrc_pixels = pBlock_pixels;
	for (uint32_t i = 0; i < total_block_pixels; i++, pSrc_pixels += 4)
	{
		float v[4];
		vec4_load_u8(v, pSrc_pixels);
				
		const float inten = 
			((v[0] - mean_r) * axis_r) + ((v[1] - mean_g) * axis_g) + 
			((v[2] - mean_b) * axis_b) + ((v[3] - mean_a) * axis_a);

		if (inten < darkest_inten)
		{
			darkest_inten = inten;
			vec4_copy(cluster_centroids[0], v);
		}

		if (inten > brightest_inten)
		{
			brightest_inten = inten;
			vec4_copy(cluster_centroids[1], v);
		}

	} // i

	float furthest_dist2 = 0.0f;

	vec4_copy(cluster_centroids[2], cluster_centroids[0]);

	pSrc_pixels = pBlock_pixels;
	for (uint32_t i = 0; i < total_block_pixels; i++, pSrc_pixels += 4)
	{
		float v[4];
		vec4_set(v, (float)pSrc_pixels[0], (float)pSrc_pixels[1], (float)pSrc_pixels[2], (float)pSrc_pixels[3]);

		float dist_a = vec4_squared_dist(v, cluster_centroids[0]);
		if (dist_a == 0.0f)
			continue;

		float dist_b = vec4_squared_dist(v, cluster_centroids[1]);
		if (dist_b == 0.0f)
			continue;

		float dist2 = dist_a + dist_b;
		if (dist2 > furthest_dist2)
		{
			furthest_dist2 = dist2;
			vec4_copy(cluster_centroids[2], v);
		}
	}

	if (vec4_compare(cluster_centroids[0], cluster_centroids[1]) ||
		vec4_compare(cluster_centroids[0], cluster_centroids[2]) ||
		vec4_compare(cluster_centroids[1], cluster_centroids[2]))
	{
		return false;
	}
		
	uint32_t num_cluster_pixels[NUM_SUBSETS];

	const uint32_t NUM_ITERS = 4;
		
	for (uint32_t s = 0; s < NUM_ITERS; s++)
	{
		float new_cluster_means[NUM_SUBSETS][4];

		clear_obj(num_cluster_pixels);
		clear_obj(new_cluster_means);

		pSrc_pixels = pBlock_pixels;

		bool changed_flag = false;

		for (uint32_t i = 0; i < total_block_pixels; i++, pSrc_pixels += 4)
		{
			float v[4];
			vec4_set(v, (float)pSrc_pixels[0], (float)pSrc_pixels[1], (float)pSrc_pixels[2], (float)pSrc_pixels[3]);

			const float d0 = vec4_squared_dist(v, cluster_centroids[0]);
			const float d1 = vec4_squared_dist(v, cluster_centroids[1]);
			const float d2 = vec4_squared_dist(v, cluster_centroids[2]);

			uint32_t idx = 0; float md = d0;
			if (d1 < md) { md = d1; idx = 1; }
			if (d2 < md) { idx = 2; }

			if (idx != pDesired_part[i])
				changed_flag = true;

			pDesired_part[i] = (uint8_t)idx;

			vec4_add(new_cluster_means[idx], v);

			num_cluster_pixels[idx]++;
		} // i

		if (!num_cluster_pixels[0] || !num_cluster_pixels[1] || !num_cluster_pixels[2])
			return false;

		if (!changed_flag)
			break;

		if (s < (NUM_ITERS - 1))
		{
			for (uint32_t j = 0; j < NUM_SUBSETS; j++)
				vec4_scale(cluster_centroids[j], new_cluster_means[j], 1.0f / (float)num_cluster_pixels[j]);
		}

	} // s

	return true;
}

static void compress_block_3subsets_internal(
	const subset_enc_context& enc_context,
	subset_enc_thread_context& enc_thread_context,
	const uint8_t* pBlock_pixels,
	double& best_lblock_error, astc_helpers::log_astc_block& best_lblock,
	astc_lblock_vec* pAll_candidates,
	single_subset_shortlist_state& shortlist_state)
{
	const uint32_t block_width = enc_context.m_block_width, block_height = enc_context.m_block_height;
	const uint32_t total_block_pixels = enc_context.m_total_block_pixels;

	const uint32_t num_src_block_chans = shortlist_state.m_num_src_block_comps;
	[[maybe_unused]] const bool has_a = (num_src_block_chans == 4);

	rgba32_image block_img;
	block_img.m_pPixels = pBlock_pixels;
	block_img.m_width = block_width;
	block_img.m_height = block_height;
	block_img.m_row_pitch_in_texels = block_width;

	const float SUBSET_SCALE_WEIGHT = 2.0f;

	const uint32_t total_candidates = generate_single_subset_shortlist(
		TOTAL_THREE_SUBSET_CONFIGS_RGBA, g_three_subset_configs_rgba,
		enc_context,
		block_img,
		shortlist_state.m_src_is_luma_only,
		shortlist_state.m_num_src_block_comps,
		shortlist_state, SUBSET_SCALE_WEIGHT, enc_context.m_num_carrier_candidates);

	if (!total_candidates)
	{
		assert(0);
		return;
	}

#if 0
	encode_single_subset_block(enc_context, shortlist_state.m_pbuf, shortlist_state.m_num_src_block_comps, shortlist_state.m_best_configs[0], best_lblock, false);

	best_lblock.m_color_endpoint_modes[1] = best_lblock.m_color_endpoint_modes[0];
	best_lblock.m_color_endpoint_modes[2] = best_lblock.m_color_endpoint_modes[0];

	const uint32_t num_endpoint_vals = astc_helpers::get_num_cem_values(best_lblock.m_color_endpoint_modes[0]);
	memcpy(best_lblock.m_endpoints + num_endpoint_vals, best_lblock.m_endpoints, num_endpoint_vals);
	memcpy(best_lblock.m_endpoints + num_endpoint_vals * 2, best_lblock.m_endpoints, num_endpoint_vals);

	best_lblock.m_num_partitions = 3;
	best_lblock.m_partition_id = 1;

	convert_rank_lblock_to_ise(best_lblock);
	return;
#endif
		
	enc_thread_context.m_pat_vec.init(enc_context.m_block_width, enc_context.m_block_height);

	if (!create_desired_partitions_3subsets(shortlist_state.m_stats, total_block_pixels, pBlock_pixels, enc_thread_context.m_pat_vec.m_parts))
		return;

	astc_ldr::partitions_data* pPart_data = enc_context.m_pPart_data_p3;

	uint32_t cand_patterns[MAX_UNIQUE_3SUBSET_PATS];
	assert(enc_context.m_num_pattern_candidates <= MAX_UNIQUE_3SUBSET_PATS);
	const uint32_t num_pat_candidates = pPart_data->m_part_lhs_map.find(enc_thread_context.m_pat_vec, cand_patterns, enc_context.m_num_pattern_candidates, false);

	if (!num_pat_candidates)
		return;

	for (uint32_t config_cand_index = 0; config_cand_index < total_candidates; config_cand_index++)
	{
		const astc_unpacked_config& best_config = shortlist_state.m_best_configs[config_cand_index];

		const bool cem_has_a = does_cem_have_alpha(best_config.m_cem);
		const uint32_t num_cem_comps = get_num_cem_chans(best_config.m_cem);

		const basist::astc_ldr_t::astc_block_grid_data* pGrid_data = basist::astc_ldr_t::find_astc_block_grid_data(block_width, block_height, best_config.m_grid_width, best_config.m_grid_height);

		astc_helpers::log_astc_block carrier_lblock; // carrier block in rank space

		encode_single_subset_block(enc_context, shortlist_state.m_pbuf, shortlist_state.m_num_src_block_comps, best_config, carrier_lblock, false);

		// base+ofs is so marginal that it's not worth the effort/extra complexity here
		assert(!is_cem_9_or_13(carrier_lblock.m_color_endpoint_modes[0]));

		convert_ise_lblock_to_rank(carrier_lblock);

		for (uint32_t pat_cand_iter = 0; pat_cand_iter < num_pat_candidates; pat_cand_iter++)
		{
			const uint32_t best_unique_pat_index = cand_patterns[pat_cand_iter];
			assert(best_unique_pat_index < pPart_data->m_total_unique_patterns);
						
			const uint32_t best_seed_id = pPart_data->m_unique_index_to_part_seed[best_unique_pat_index];
			assert((int)best_unique_pat_index == pPart_data->m_part_seed_to_unique_index[best_seed_id]);
				
			float subset_pixels[3][PIXELBUF_SIZE_IN_FLOATS];

			pixelbuf subset_pbuf[3];
			subset_pbuf[0].m_pBuf = subset_pixels[0];
			subset_pbuf[0].m_width = 0;
			subset_pbuf[0].m_height = 1;

			subset_pbuf[1].m_pBuf = subset_pixels[1];
			subset_pbuf[1].m_width = 0;
			subset_pbuf[1].m_height = 1;

			subset_pbuf[2].m_pBuf = subset_pixels[2];
			subset_pbuf[2].m_width = 0;
			subset_pbuf[2].m_height = 1;

			uint32_t subset_pixel_indices[3][256];

			const uint8_t* pSrc_pixels = pBlock_pixels;
			[[maybe_unused]] uint32_t bit_ofs = 0;

			for (uint32_t y = 0; y < block_height; y++)
			{
				for (uint32_t x = 0; x < block_width; x++)
				{
					const uint32_t s = pPart_data->m_partition_pats[best_unique_pat_index](x, y);
					assert(s < 3);

					const float r = (float)pSrc_pixels[0];
					const float g = (float)pSrc_pixels[1];
					const float b = (float)pSrc_pixels[2];
					const float a = cem_has_a ? (float)pSrc_pixels[3] : 255;
					pSrc_pixels += 4;

					const uint32_t cur_width = subset_pbuf[s].m_width;

					subset_pixel_indices[s][cur_width] = x + y * block_width;

					pixelbuf_set_comp(subset_pbuf[s], cur_width, 0, 0, r);
					pixelbuf_set_comp(subset_pbuf[s], cur_width, 0, 1, g);
					pixelbuf_set_comp(subset_pbuf[s], cur_width, 0, 2, b);
					pixelbuf_set_comp(subset_pbuf[s], cur_width, 0, 3, a);

					subset_pbuf[s].m_width = cur_width + 1;

				} // x

			} // y

			assert(subset_pbuf[0].m_width && subset_pbuf[1].m_width && subset_pbuf[2].m_width);

			// -------

			astc_helpers::log_astc_block final_lblock;
			final_lblock.clear();
			final_lblock.m_user_mode = cUserModeRankValues;

			final_lblock.m_grid_width = carrier_lblock.m_grid_width;
			final_lblock.m_grid_height = carrier_lblock.m_grid_height;
			final_lblock.m_endpoint_ise_range = best_config.m_endpoint_range;
			final_lblock.m_weight_ise_range = best_config.m_weight_range;
			final_lblock.m_num_partitions = 3;
			final_lblock.m_partition_id = safe_cast_uint16(best_seed_id);
			final_lblock.m_color_endpoint_modes[0] = best_config.m_cem;
			final_lblock.m_color_endpoint_modes[1] = best_config.m_cem;
			final_lblock.m_color_endpoint_modes[2] = best_config.m_cem;

			// -------

			const uint32_t num_cem_endpoint_vals = astc_helpers::get_num_cem_values(best_config.m_cem);
			const uint32_t total_grid_weights = carrier_lblock.m_grid_width * carrier_lblock.m_grid_height;

			//const auto& endpoint_tab = astc_helpers::g_dequant_tables.get_endpoint_tab(best_config.m_endpoint_range);
			const auto& weight_tab = astc_helpers::g_dequant_tables.get_weight_tab(best_config.m_weight_range);

			[[maybe_unused]] const uint32_t num_weight_levels = astc_helpers::get_ise_levels(best_config.m_weight_range);

			// ------- method 1
			if (enc_context.m_use_method1)
			{
				memcpy(final_lblock.m_weights, carrier_lblock.m_weights, total_grid_weights);
				memcpy(final_lblock.m_endpoints, carrier_lblock.m_endpoints, num_cem_endpoint_vals);
				memcpy(final_lblock.m_endpoints + num_cem_endpoint_vals, carrier_lblock.m_endpoints, num_cem_endpoint_vals);
				memcpy(final_lblock.m_endpoints + 2 * num_cem_endpoint_vals, carrier_lblock.m_endpoints, num_cem_endpoint_vals);	

				const uint32_t NUM_M1_PASSES = 2;

				for (uint32_t pass = 0; pass < NUM_M1_PASSES; pass++)
				{
					refine_endpoints_with_current_weights(shortlist_state.m_pbuf, final_lblock, subset_pbuf, subset_pixel_indices, best_config, enc_context,
						best_lblock_error, best_lblock, nullptr);

					bool changed_flag;

					if (pass == (NUM_M1_PASSES - 1))
					{
						changed_flag = weight_polish(shortlist_state.m_pbuf, final_lblock, enc_context, pGrid_data,
							best_lblock_error, best_lblock, nullptr);
					}
					else
					{
						changed_flag = weight_gradient_descent(shortlist_state.m_pbuf, final_lblock, subset_pbuf, subset_pixel_indices, best_config, enc_context, pGrid_data,
							best_lblock_error, best_lblock, nullptr);
					}

					if (!changed_flag)
					{
						break;
					}
				}

				if (pAll_candidates)
				{
					pAll_candidates->push_back(final_lblock);
				}
			}

			double best_pat_lblock_error = DBL_MAX;
			astc_helpers::log_astc_block best_pat_lblock;

			// ------- method 2
			if (enc_context.m_use_method2)
			{
				uint8_t desired_block_weights[astc_helpers::MAX_BLOCK_PIXELS]; // at block res, ranks

#if defined(DEBUG) || defined(_DEBUG)            
				memset(desired_block_weights, 0xFF, astc_helpers::MAX_BLOCK_PIXELS);
#endif					
				for (uint32_t s = 0; s < 3; s++)
				{
					float initial_endpoints[8];
					calc_initial_cem_endpoints(subset_pbuf[s], initial_endpoints, num_cem_comps, nullptr, is_cem_6_or_10(best_config.m_cem));

					uint8_t* pSubset_CEM_vals = final_lblock.m_endpoints + s * num_cem_endpoint_vals;
					cem_encode(best_config.m_cem, initial_endpoints, best_config.m_endpoint_range, pSubset_CEM_vals, true, enc_context.m_higher_effort_bc);

					uint8_t subset_weights[astc_helpers::MAX_BLOCK_PIXELS];
					eval_weights_first_plane(subset_pbuf[s], subset_weights, best_config.m_weight_range,
						best_config.m_cem, pSubset_CEM_vals, best_config.m_endpoint_range,
						num_cem_comps);

					for (uint32_t i = 0; i < enc_context.m_num_ls_iterations; i++)
					{
						float refined_endpoints[8];
						refine_endpoints_given_weights(subset_pbuf[s], best_config.m_cem,
							subset_weights, best_config.m_weight_range, pSubset_CEM_vals, best_config.m_endpoint_range, refined_endpoints, num_cem_comps);

						cem_encode(best_config.m_cem, refined_endpoints, best_config.m_endpoint_range, pSubset_CEM_vals, true, enc_context.m_higher_effort_bc);

						eval_weights_first_plane(subset_pbuf[s], subset_weights, best_config.m_weight_range,
							best_config.m_cem, pSubset_CEM_vals, best_config.m_endpoint_range,
							num_cem_comps);
					} // i

					// base+ofs is so rarely a win (< ~1%) that it's not worth the trouble

					for (uint32_t j = 0; j < subset_pbuf[s].m_width; j++)
					{
						const uint32_t pixel_index = subset_pixel_indices[s][j];
						assert(pixel_index < total_block_pixels);

						desired_block_weights[pixel_index] = subset_weights[j];
					}

				} // s

#if defined(DEBUG) || defined(_DEBUG)
				for (uint32_t i = 0; i < total_block_pixels; i++)
				{
					assert(desired_block_weights[i] != 0xFF);
				}
#endif

				// now downsample the ideal weights to grid res, quantize
				// desired_block_weights[] is either [0,64] (dequantized weight values) or in the carrier's weight rank space depending on solve_weights_dequantized

				if ((block_width == best_config.m_grid_width) && (block_height == best_config.m_grid_height))
				{
					assert(total_block_pixels == total_grid_weights);

					memcpy(final_lblock.m_weights, desired_block_weights, total_block_pixels);
				}
				else
				{
					// dequantize ranks to [0,64] values
					for (uint32_t i = 0; i < total_block_pixels; i++)
						desired_block_weights[i] = (uint8_t)weight_tab.get_rank_to_val(desired_block_weights[i]);

					// downsample from block to grid res, [0,64] values
					uint8_t downsampled_weights[astc_helpers::MAX_GRID_WEIGHTS];

#if 0
					basisu::downsample_weight_grid(pGrid_data->m_downsample_matrix.get_ptr(),
						block_width, block_height,
						best_config.m_grid_width, best_config.m_grid_height,
						desired_block_weights,
						downsampled_weights);
#else
					float src_temp[PIXELBUF_COMP_PITCH];
					pixelbuf src_pbuf(block_width, block_height, src_temp);
					for (uint32_t y = 0; y < block_height; y++)
						for (uint32_t x = 0; x < block_width; x++)
							pixelbuf_set_comp(src_pbuf, x, y, 0, desired_block_weights[x + y * block_width]);

					float dst_temp[PIXELBUF_COMP_PITCH];
					pixelbuf dst_pbuf(best_config.m_grid_width, best_config.m_grid_height, dst_temp);

					pseudoinverse_block_to_grid(src_pbuf, dst_pbuf, 1);
					for (uint32_t y = 0; y < best_config.m_grid_height; y++)
						for (uint32_t x = 0; x < best_config.m_grid_width; x++)
							downsampled_weights[x + y * best_config.m_grid_width] = (uint8_t)clamp((int)std::round(pixelbuf_get_comp(dst_pbuf, x, y, 0)), 0, 64);
#endif

					// quantize downsampled weights
					for (uint32_t i = 0; i < total_grid_weights; i++)
						final_lblock.m_weights[i] = (uint8_t)weight_tab.get_val_to_rank(downsampled_weights[i]);
				}

				{
					// convert from rank to ISE space
					const double subset_error = compute_block_error(final_lblock, shortlist_state.m_pbuf, enc_context);

					if (subset_error < best_pat_lblock_error)
					{
						best_pat_lblock_error = subset_error;
						best_pat_lblock = final_lblock;
					}
				}

				const uint32_t NUM_M2_PASSES = 2;

				for (uint32_t pass = 0; pass < NUM_M2_PASSES; pass++)
				{
					refine_endpoints_with_current_weights(shortlist_state.m_pbuf, final_lblock, subset_pbuf, subset_pixel_indices, best_config, enc_context,
						best_pat_lblock_error, best_pat_lblock, nullptr);

					bool changed_flag;

					if (pass == (NUM_M2_PASSES - 1))
					{
						changed_flag = weight_polish(shortlist_state.m_pbuf, final_lblock, enc_context, pGrid_data,
							best_pat_lblock_error, best_pat_lblock, nullptr);
					}
					else
					{
						changed_flag = weight_gradient_descent(shortlist_state.m_pbuf, final_lblock, subset_pbuf, subset_pixel_indices, best_config, enc_context, pGrid_data,
							best_pat_lblock_error, best_pat_lblock, nullptr);
					}

					if (!changed_flag)
					{
						break;
					}
				}

				assert(best_pat_lblock_error != DBL_MAX);
				if (best_pat_lblock_error < best_lblock_error)
				{
					best_lblock_error = best_pat_lblock_error;
					best_lblock = best_pat_lblock;
				}

				if (pAll_candidates)
				{
					pAll_candidates->push_back(best_pat_lblock);
				}
			}

		} // pat_cand_iter

	} // config_cand_index

	convert_rank_lblock_to_ise(best_lblock);
}

double compress_block_subsets(
	const subset_enc_context& enc_context,
	subset_enc_thread_context& enc_thread_context,
	const uint8_t* pBlock_pixels,
	astc_helpers::log_astc_block& best_lblock, astc_lblock_vec* pAll_candidates)
{
	assert(enc_context.m_use_method1 || enc_context.m_use_method2);

	const bool FORCE_SUBSETS = false;
		
	single_subset_shortlist_state shortlist_state;
	double best_lblock_error = compress_single_subset_internal(enc_context, pBlock_pixels, best_lblock, pAll_candidates, shortlist_state, true, DEF_SCALE_WEIGHT);

	if ((best_lblock.m_solid_color_flag_ldr) || (enc_context.m_max_subsets == 1))
		return best_lblock_error;

	assert(is_lblock_ise(best_lblock));

	if (FORCE_SUBSETS)
		best_lblock_error = DBL_MAX;

	const float max_chan_var = FORCE_SUBSETS ? FLT_MAX : (basisu::maximum<float>(
		shortlist_state.m_stats.m_covar[cCovarRR], shortlist_state.m_stats.m_covar[cCovarGG],
		shortlist_state.m_stats.m_covar[cCovarBB], shortlist_state.m_stats.m_covar[cCovarAA]) / (float)enc_context.m_total_block_pixels); // TODO: divide

	if (max_chan_var >= enc_context.m_two_subset_var_thresh)
	{
		compress_block_2subsets_internal(
			enc_context,
			pBlock_pixels,
			best_lblock_error, best_lblock,
			pAll_candidates, shortlist_state);
	}

	if ((enc_context.m_max_subsets == 3) && (max_chan_var >= enc_context.m_three_subset_var_thresh))
	{
		compress_block_3subsets_internal(
			enc_context, enc_thread_context,
			pBlock_pixels,
			best_lblock_error, best_lblock,
			pAll_candidates, shortlist_state);
	}

	return best_lblock_error;
}

} // namespace astc_ldrf
} // namespace basisu
