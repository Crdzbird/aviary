package aviary;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.nio.charset.StandardCharsets;
import static java.lang.foreign.ValueLayout.*;

/**
 * Aviary JVM binding (Java FFM / Panama). Drives the full node - create a
 * chain, accounts, transfer, produce blocks, stream events, metrics, light-client
 * verification - plus EVM/Bitcoin/Solana interop, over the C ABI. Usable from
 * Java and Kotlin alike.
 */
public final class Aviary implements AutoCloseable {
    private final Arena arena = Arena.ofShared();
    private final Linker linker = Linker.nativeLinker();
    private final SymbolLookup lib;

    static final GroupLayout BUF = MemoryLayout.structLayout(ADDRESS.withName("data"), JAVA_LONG.withName("len"));
    static final GroupLayout RES = MemoryLayout.structLayout(BUF.withName("buf"), ADDRESS.withName("error"));
    static final long B = BUF.byteSize();

    private final MethodHandle init, version, backend, nodeNew, nodeFree, newAccount, addr, credit,
            submit, submitFee, produce, height, balance, nonce, mempool, stateRoot, drain, metrics, prove,
            verifyProof, ethAddr, btcAddr, solAddr, bufFree, strFree,
            newAccountHybrid, submitHybrid,
            ksNew, ksAdd, ksReveal, ksSerialise, ksLoad, ksUnlock, ksUnlocked, ksFree,
            kemKeygen, pSealPass, pOpenPass, pSealTo, pOpenTo, pSealSigned, pOpenSigned,
            thCreate, thInfo, thSeal, thUnseal, thCommit, thCerNew, thCerFree, thSign, thVerify, thAgg,
            asDescribe, txEncode, smSeal, smNew, smAdd, smSize, smReveal, smFree,
            bbsKeygen, credPolicyEncode, credPolicyHash, credIssue, credPresent,
            credGateNew, credGateRevoke, credGateAdmit, credGateFree,
            credPolicyEncodeEx, membershipCommit, blindIssue, finalizeMembership,
            credPresentPseud, credGateAdmitPseud, credGateRevokeNym, credGateNoteAdmission,
            blsKeygen, bpEncode, bpHash, registryIds, genesisBuild, genesisCommitBp, genesisHash,
            chainAssemble, chainHeight, chainBlueprint, chainPropose, chainSeal, chainMine, chainSubmit, chainFree, chainDrainEvents, chainSetEventHook, chainMetrics,
            regAddEngine, regAddProposer,
            genesisHeaderH, childRegEncode, subchainPolicyEncode, genesisCommitSubchains, genesisCommitBinding,
            blockHeaderH, headerSubchainRootH, certifiedHeaderEncode, anchorEncode, chainProposeAnchored, chainCertify,
            chainChildTip, chainSubchainHop, chainProveAccount, subchainProofEncode, subchainProofVerify,
            chainScheduleValidators, chainValidatorsH,
            lcNew, lcAccept, lcAcceptSkipping, lcHeight, lcTip, lcValidators, lcSetTrust, lcSetMaxSkip, lcFree,
            treeNodeEncode, treeSpecError, treeBuild, treeChainIds, treeHeight, treeGenesis, treeTip, treePending, treeProduce,
            treeProve, treeVerify, treeSetTxSource, treeSetFinalized, treeFree;
    private final MethodHandle nodeSetEventHook, nodeSnapshot, nodeRestore, nodeRestoreReplay, nodeNextNonce, nodeBlockAt, nodeSubmitTx, nodeSubmitKs,
            ksSign, ksPublicKey, ksHas, ksAddresses, ksLock,
            netNew, netAddPeer, netSetTxs, netStart, netOnGossip, netOnTimeout, netAdvance, netCommitted, netHeight,
            netPeerBanned, netReplayLog, netTip, netStateRoot, netBalance, netProve, netFree, netDrainEvents, netSetEventHook, netMetrics,
            secIdentity, secInitiate, secAccept, secHandshake, secComplete, secOpen, secPeer, secEncrypt, secDecrypt, secFree;

    public Aviary() {
        lib = SymbolLookup.libraryLookup(NativeLoader.resolve(), arena);
        init       = h("aviary_init", FunctionDescriptor.of(JAVA_INT));
        version    = h("aviary_version", FunctionDescriptor.of(ADDRESS));
        backend    = h("aviary_crypto_backend_version", FunctionDescriptor.of(ADDRESS));
        nodeNew    = h("aviary_node_new", FunctionDescriptor.of(ADDRESS, JAVA_LONG));
        nodeFree   = h("aviary_node_free", FunctionDescriptor.ofVoid(ADDRESS));
        newAccount = h("aviary_node_new_account", FunctionDescriptor.of(RES, ADDRESS));
        addr       = h("aviary_account_address", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        credit     = h("aviary_node_credit", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG));
        submit     = h("aviary_node_submit_transfer", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_LONG));
        submitFee  = h("aviary_node_submit_transfer_fee", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_LONG, JAVA_LONG));
        newAccountHybrid = h("aviary_node_new_account_hybrid", FunctionDescriptor.of(RES, ADDRESS));
        submitHybrid = h("aviary_node_submit_transfer_hybrid", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_LONG));
        produce    = h("aviary_node_produce_block", FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS));
        height     = h("aviary_node_height", FunctionDescriptor.of(JAVA_LONG, ADDRESS));
        balance    = h("aviary_node_balance", FunctionDescriptor.of(JAVA_LONG, ADDRESS, ADDRESS));
        nonce      = h("aviary_node_nonce", FunctionDescriptor.of(JAVA_LONG, ADDRESS, ADDRESS));
        mempool    = h("aviary_node_mempool_size", FunctionDescriptor.of(JAVA_LONG, ADDRESS));
        stateRoot  = h("aviary_node_state_root", FunctionDescriptor.of(RES, ADDRESS));
        drain      = h("aviary_node_drain_events", FunctionDescriptor.of(RES, ADDRESS));
        metrics    = h("aviary_node_metrics", FunctionDescriptor.of(RES, ADDRESS));
        prove      = h("aviary_node_prove_account", FunctionDescriptor.of(RES, ADDRESS, ADDRESS));
        verifyProof= h("aviary_verify_state_proof", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG));
        ethAddr    = h("aviary_eth_address", FunctionDescriptor.of(RES, ADDRESS));
        btcAddr    = h("aviary_btc_address", FunctionDescriptor.of(RES, ADDRESS));
        solAddr    = h("aviary_sol_address", FunctionDescriptor.of(RES, ADDRESS));
        bufFree    = h("aviary_buffer_free", FunctionDescriptor.ofVoid(ADDRESS));
        strFree    = h("aviary_str_free", FunctionDescriptor.ofVoid(ADDRESS));
        ksNew      = h("aviary_keystore_new", FunctionDescriptor.of(ADDRESS, ADDRESS));
        ksAdd      = h("aviary_keystore_add_account", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS));
        ksReveal   = h("aviary_keystore_reveal", FunctionDescriptor.of(RES, ADDRESS, ADDRESS));
        ksSerialise= h("aviary_keystore_serialise", FunctionDescriptor.of(RES, ADDRESS));
        ksLoad     = h("aviary_keystore_load", FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG));
        ksUnlock   = h("aviary_keystore_unlock", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS));
        ksUnlocked = h("aviary_keystore_unlocked", FunctionDescriptor.of(JAVA_INT, ADDRESS));
        ksFree     = h("aviary_keystore_free", FunctionDescriptor.ofVoid(ADDRESS));
        kemKeygen  = h("aviary_hybrid_kem_keygen", FunctionDescriptor.of(RES));
        pSealPass  = h("aviary_portable_seal_passphrase", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_LONG));
        pOpenPass  = h("aviary_portable_open_passphrase", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS));
        pSealTo    = h("aviary_portable_seal_to", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        pOpenTo    = h("aviary_portable_open_to", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        pSealSigned= h("aviary_portable_seal_signed_to", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        pOpenSigned= h("aviary_portable_open_signed_from", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        thCreate   = h("aviary_threshold_create", FunctionDescriptor.of(RES, JAVA_SHORT, JAVA_SHORT));
        thInfo     = h("aviary_threshold_share_info", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        thSeal     = h("aviary_threshold_share_seal", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_LONG));
        thUnseal   = h("aviary_threshold_share_unseal", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS));
        thCommit   = h("aviary_threshold_commit", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        thCerNew   = h("aviary_threshold_ceremony_new", FunctionDescriptor.of(ADDRESS, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        thCerFree  = h("aviary_threshold_ceremony_free", FunctionDescriptor.ofVoid(ADDRESS));
        thSign     = h("aviary_threshold_sign", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS));
        thVerify   = h("aviary_threshold_verify_share", FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_SHORT, ADDRESS, ADDRESS));
        thAgg      = h("aviary_threshold_aggregate", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, ADDRESS, JAVA_LONG));
        asDescribe = h("aviary_assurance_describe", FunctionDescriptor.of(RES, JAVA_SHORT));
        txEncode   = h("aviary_tx_encode", FunctionDescriptor.of(RES, JAVA_BYTE, JAVA_LONG, JAVA_BYTE, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        smSeal     = h("aviary_sealed_tx_seal", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        smNew      = h("aviary_sealed_mempool_new", FunctionDescriptor.of(ADDRESS, JAVA_LONG));
        smAdd      = h("aviary_sealed_mempool_add", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG));
        smSize     = h("aviary_sealed_mempool_size", FunctionDescriptor.of(JAVA_LONG, ADDRESS));
        smReveal   = h("aviary_sealed_mempool_reveal", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, JAVA_LONG, JAVA_LONG));
        smFree     = h("aviary_sealed_mempool_free", FunctionDescriptor.ofVoid(ADDRESS));
        bbsKeygen  = h("aviary_bbs_keygen", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        credPolicyEncode = h("aviary_credential_policy_encode", FunctionDescriptor.of(RES, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_INT, JAVA_BYTE));
        credPolicyHash = h("aviary_credential_policy_hash", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        credIssue  = h("aviary_credential_issue", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_BYTE, JAVA_LONG, JAVA_INT));
        credPresent = h("aviary_credential_present", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG));
        credGateNew = h("aviary_credential_gate_new", FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG));
        credGateRevoke = h("aviary_credential_gate_revoke_epoch", FunctionDescriptor.ofVoid(ADDRESS, JAVA_INT));
        credGateAdmit = h("aviary_credential_gate_admit", FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_LONG));
        credGateFree = h("aviary_credential_gate_free", FunctionDescriptor.ofVoid(ADDRESS));
        credPolicyEncodeEx = h("aviary_credential_policy_encode_ex", FunctionDescriptor.of(RES, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_INT, JAVA_BYTE, JAVA_BYTE));
        membershipCommit = h("aviary_membership_commit", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        blindIssue = h("aviary_blind_issue", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_BYTE, JAVA_LONG, JAVA_INT));
        finalizeMembership = h("aviary_finalize_membership", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        credPresentPseud = h("aviary_credential_present_pseudonymous", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG));
        credGateAdmitPseud = h("aviary_credential_gate_admit_pseudonymous", FunctionDescriptor.of(RES, ADDRESS, JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_LONG));
        credGateRevokeNym = h("aviary_credential_gate_revoke_nym", FunctionDescriptor.ofVoid(ADDRESS, ADDRESS, JAVA_LONG));
        credGateNoteAdmission = h("aviary_credential_gate_note_admission", FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG));
        blsKeygen  = h("aviary_bls_keygen", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        bpEncode   = h("aviary_blueprint_encode", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, ADDRESS, ADDRESS, JAVA_LONG));
        bpHash     = h("aviary_blueprint_hash", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        registryIds = h("aviary_registry_ids", FunctionDescriptor.of(RES, JAVA_BYTE));
        genesisBuild = h("aviary_genesis_build", FunctionDescriptor.of(RES, JAVA_LONG, JAVA_LONG, JAVA_SHORT, JAVA_BYTE, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS, JAVA_LONG));
        genesisCommitBp = h("aviary_genesis_commit_blueprint", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        genesisHash = h("aviary_genesis_hash", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        chainAssemble = h("aviary_chain_assemble", FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG, JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        chainHeight = h("aviary_chain_height", FunctionDescriptor.of(JAVA_LONG, ADDRESS));
        chainBlueprint = h("aviary_chain_blueprint", FunctionDescriptor.of(RES, ADDRESS));
        chainPropose = h("aviary_chain_propose", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        chainSeal  = h("aviary_chain_seal", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, JAVA_LONG));
        chainMine  = h("aviary_chain_mine", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, JAVA_LONG));
        chainSubmit = h("aviary_chain_submit", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        chainFree  = h("aviary_chain_free", FunctionDescriptor.ofVoid(ADDRESS));
        chainDrainEvents = h("aviary_chain_drain_events", FunctionDescriptor.of(RES, ADDRESS));
        chainSetEventHook = h("aviary_chain_set_event_hook", FunctionDescriptor.ofVoid(ADDRESS, ADDRESS, ADDRESS));
        chainMetrics = h("aviary_chain_metrics", FunctionDescriptor.of(RES, ADDRESS));
        regAddEngine = h("aviary_registry_add_engine", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, ADDRESS, ADDRESS, JAVA_INT));
        regAddProposer = h("aviary_registry_add_proposer", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, ADDRESS));
        genesisHeaderH = h("aviary_genesis_header", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        childRegEncode = h("aviary_child_registration_encode", FunctionDescriptor.of(RES, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS, JAVA_LONG, JAVA_BYTE));
        subchainPolicyEncode = h("aviary_subchain_policy_encode", FunctionDescriptor.of(RES, JAVA_SHORT, JAVA_BYTE, JAVA_SHORT, JAVA_SHORT, ADDRESS, JAVA_LONG));
        genesisCommitSubchains = h("aviary_genesis_commit_subchain_policy", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        genesisCommitBinding = h("aviary_genesis_commit_parent_binding", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, JAVA_LONG, ADDRESS));
        blockHeaderH = h("aviary_block_header", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        headerSubchainRootH = h("aviary_header_subchain_root", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        certifiedHeaderEncode = h("aviary_certified_header_encode", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        anchorEncode = h("aviary_anchor_encode", FunctionDescriptor.of(RES, JAVA_LONG, ADDRESS, JAVA_LONG));
        chainProposeAnchored = h("aviary_chain_propose_anchored", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        chainCertify = h("aviary_chain_certify", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, JAVA_LONG));
        chainChildTip = h("aviary_chain_child_tip", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        chainSubchainHop = h("aviary_chain_subchain_hop", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        chainProveAccount = h("aviary_chain_prove_account", FunctionDescriptor.of(RES, ADDRESS, ADDRESS));
        subchainProofEncode = h("aviary_subchain_proof_encode", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        subchainProofVerify = h("aviary_subchain_proof_verify", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG));
        chainScheduleValidators = h("aviary_chain_schedule_validators", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, ADDRESS, JAVA_LONG));
        chainValidatorsH = h("aviary_chain_validators", FunctionDescriptor.of(RES, ADDRESS));
        lcNew = h("aviary_light_client_new", FunctionDescriptor.of(ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        lcAccept = h("aviary_light_client_accept", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        lcAcceptSkipping = h("aviary_light_client_accept_skipping", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        lcHeight = h("aviary_light_client_height", FunctionDescriptor.of(JAVA_LONG, ADDRESS));
        lcTip = h("aviary_light_client_tip", FunctionDescriptor.of(RES, ADDRESS));
        lcValidators = h("aviary_light_client_validators", FunctionDescriptor.of(RES, ADDRESS));
        lcSetTrust = h("aviary_light_client_set_trust_level", FunctionDescriptor.ofVoid(ADDRESS, JAVA_INT, JAVA_INT));
        lcSetMaxSkip = h("aviary_light_client_set_max_skip", FunctionDescriptor.ofVoid(ADDRESS, JAVA_LONG));
        lcFree = h("aviary_light_client_free", FunctionDescriptor.ofVoid(ADDRESS));
        treeNodeEncode = h("aviary_chain_tree_node_encode", FunctionDescriptor.of(RES, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_SHORT, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS, ADDRESS, JAVA_LONG, JAVA_SHORT, JAVA_SHORT, ADDRESS, JAVA_LONG));
        treeSpecError = h("aviary_chain_tree_spec_error", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        treeBuild = h("aviary_chain_tree_build", FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG));
        treeChainIds = h("aviary_chain_tree_chain_ids", FunctionDescriptor.of(RES, ADDRESS));
        treeHeight = h("aviary_chain_tree_height", FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG));
        treeGenesis = h("aviary_chain_tree_genesis", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        treeTip = h("aviary_chain_tree_tip", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        treePending = h("aviary_chain_tree_pending_anchors", FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_LONG));
        treeProduce = h("aviary_chain_tree_produce", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, JAVA_LONG));
        treeProve = h("aviary_chain_tree_prove_account", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS));
        treeVerify = h("aviary_chain_tree_verify", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG));
        treeSetTxSource = h("aviary_chain_tree_set_tx_source", FunctionDescriptor.ofVoid(ADDRESS, ADDRESS, ADDRESS));
        treeSetFinalized = h("aviary_chain_tree_set_finalized_hook", FunctionDescriptor.ofVoid(ADDRESS, ADDRESS, ADDRESS));
        treeFree = h("aviary_chain_tree_free", FunctionDescriptor.ofVoid(ADDRESS));
        nodeSetEventHook = h("aviary_node_set_event_hook", FunctionDescriptor.ofVoid(ADDRESS, ADDRESS, ADDRESS));
        nodeSnapshot = h("aviary_node_snapshot", FunctionDescriptor.of(RES, ADDRESS));
        nodeRestore = h("aviary_node_restore", FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG));
        nodeRestoreReplay = h("aviary_node_restore_replay", FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG));
        nodeNextNonce = h("aviary_node_next_nonce", FunctionDescriptor.of(JAVA_LONG, ADDRESS, ADDRESS));
        nodeBlockAt = h("aviary_node_block_at", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG));
        nodeSubmitTx = h("aviary_node_submit_tx", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, JAVA_LONG));
        nodeSubmitKs = h("aviary_node_submit_transfer_keystore", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, ADDRESS, ADDRESS, JAVA_LONG, JAVA_LONG, JAVA_LONG));
        ksSign = h("aviary_keystore_sign_transfer", FunctionDescriptor.of(RES, ADDRESS, JAVA_LONG, ADDRESS, ADDRESS, JAVA_LONG, JAVA_LONG, JAVA_LONG));
        ksPublicKey = h("aviary_keystore_public_key", FunctionDescriptor.of(RES, ADDRESS, ADDRESS));
        ksHas = h("aviary_keystore_has", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS));
        ksAddresses = h("aviary_keystore_addresses", FunctionDescriptor.of(RES, ADDRESS));
        ksLock = h("aviary_keystore_lock", FunctionDescriptor.ofVoid(ADDRESS));
        netNew = h("aviary_net_node_new", FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG, JAVA_INT, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, JAVA_LONG));
        netAddPeer = h("aviary_net_node_add_peer", FunctionDescriptor.ofVoid(ADDRESS, JAVA_INT));
        netSetTxs = h("aviary_net_node_set_transactions", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG));
        netStart = h("aviary_net_node_start", FunctionDescriptor.of(RES, ADDRESS));
        netOnGossip = h("aviary_net_node_on_gossip", FunctionDescriptor.of(RES, ADDRESS, JAVA_INT, ADDRESS, JAVA_LONG));
        netOnTimeout = h("aviary_net_node_on_timeout", FunctionDescriptor.of(RES, ADDRESS, JAVA_BYTE, JAVA_INT));
        netAdvance = h("aviary_net_node_advance", FunctionDescriptor.of(RES, ADDRESS));
        netCommitted = h("aviary_net_node_committed", FunctionDescriptor.of(JAVA_INT, ADDRESS));
        netHeight = h("aviary_net_node_height", FunctionDescriptor.of(JAVA_LONG, ADDRESS));
        netPeerBanned = h("aviary_net_node_peer_banned", FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_INT));
        netReplayLog = h("aviary_net_node_replay_log", FunctionDescriptor.of(RES, ADDRESS));
        netTip = h("aviary_net_node_tip", FunctionDescriptor.of(RES, ADDRESS));
        netStateRoot = h("aviary_net_node_state_root", FunctionDescriptor.of(RES, ADDRESS));
        netBalance = h("aviary_net_node_balance", FunctionDescriptor.of(JAVA_LONG, ADDRESS, ADDRESS));
        netProve = h("aviary_net_node_prove_account", FunctionDescriptor.of(RES, ADDRESS, ADDRESS));
        netFree = h("aviary_net_node_free", FunctionDescriptor.ofVoid(ADDRESS));
        netDrainEvents = h("aviary_net_node_drain_events", FunctionDescriptor.of(RES, ADDRESS));
        netSetEventHook = h("aviary_net_node_set_event_hook", FunctionDescriptor.ofVoid(ADDRESS, ADDRESS, ADDRESS));
        netMetrics = h("aviary_net_node_metrics", FunctionDescriptor.of(RES, ADDRESS));
        secIdentity = h("aviary_secure_identity_generate", FunctionDescriptor.of(RES));
        secInitiate = h("aviary_secure_initiate", FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        secAccept = h("aviary_secure_accept", FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        secHandshake = h("aviary_secure_handshake_message", FunctionDescriptor.of(RES, ADDRESS));
        secComplete = h("aviary_secure_complete", FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG));
        secOpen = h("aviary_secure_open", FunctionDescriptor.of(JAVA_INT, ADDRESS));
        secPeer = h("aviary_secure_peer_identity", FunctionDescriptor.of(RES, ADDRESS));
        secEncrypt = h("aviary_secure_encrypt", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, JAVA_LONG));
        secDecrypt = h("aviary_secure_decrypt", FunctionDescriptor.of(RES, ADDRESS, ADDRESS, JAVA_LONG));
        secFree = h("aviary_secure_free", FunctionDescriptor.ofVoid(ADDRESS));
        try { if ((int) init.invoke() != 0) throw new IllegalStateException("init failed"); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }

    public record Account(byte[] secret, byte[] publicKey, byte[] address) {}

    public String version()        { return cstr(invokePtr(version)); }
    public String cryptoBackend()  { return cstr(invokePtr(backend)); }

    public String ethAddress(byte[] secret32) { return new String(call1(ethAddr, secret32), StandardCharsets.UTF_8); }
    public String btcAddress(byte[] secret32) { return new String(call1(btcAddr, secret32), StandardCharsets.UTF_8); }
    public String solAddress(byte[] edSecret) { return new String(call1(solAddr, edSecret), StandardCharsets.UTF_8); }

    // -- Portable account export (tiered encrypted backup) ----------------------

    /** A signing account's material (or a KEM device key), returned by the open calls. */
    public record PortableAccount(byte[] publicKey, byte[] secret) {}
    public record ValidatorEntry(byte[] publicKey, long power) {}

    private PortableAccount splitBlob(byte[] blob) {
        int pl = ((blob[0] & 0xFF) << 24) | ((blob[1] & 0xFF) << 16) | ((blob[2] & 0xFF) << 8) | (blob[3] & 0xFF);
        return new PortableAccount(java.util.Arrays.copyOfRange(blob, 4, 4 + pl),
                                   java.util.Arrays.copyOfRange(blob, 4 + pl, blob.length));
    }

    /** A hybrid-KEM device keypair for the recipient-bound export tiers. */
    public PortableAccount hybridKemKeypair() {
        try { return splitBlob(consume((MemorySegment) kemKeygen.invoke(arena.allocate(RES)))); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }

    /** Tier 1 - seal an account under a passphrase (ops=mem=0 -> SENSITIVE preset). */
    public byte[] sealAccountPassphrase(byte[] pub, byte[] secret, String passphrase, long ops, long mem) {
        try { return consume((MemorySegment) pSealPass.invoke(arena.allocate(RES), seg(pub), (long) pub.length,
                seg(secret), (long) secret.length, arena.allocateFrom(passphrase), ops, mem)); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }
    public PortableAccount openAccountPassphrase(byte[] envelope, String passphrase) {
        try { return splitBlob(consume((MemorySegment) pOpenPass.invoke(arena.allocate(RES),
                seg(envelope), (long) envelope.length, arena.allocateFrom(passphrase)))); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }

    /** Tier 2 - recipient-bound (PQ): only the device's KEM secret opens it. */
    public byte[] sealAccountTo(byte[] pub, byte[] secret, byte[] recipientKemPublic) {
        try { return consume((MemorySegment) pSealTo.invoke(arena.allocate(RES), seg(pub), (long) pub.length,
                seg(secret), (long) secret.length, seg(recipientKemPublic), (long) recipientKemPublic.length)); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }
    public PortableAccount openAccountTo(byte[] envelope, byte[] recipientKemSecret) {
        try { return splitBlob(consume((MemorySegment) pOpenTo.invoke(arena.allocate(RES),
                seg(envelope), (long) envelope.length, seg(recipientKemSecret), (long) recipientKemSecret.length))); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }

    /** Tier 3 - signed-recipient (Fortress-grade): recipient-bound + the receiver verifies the owner. */
    public byte[] sealAccountSignedTo(byte[] pub, byte[] secret, byte[] recipientKemPublic, byte[] senderSigSecret) {
        try { return consume((MemorySegment) pSealSigned.invoke(arena.allocate(RES), seg(pub), (long) pub.length,
                seg(secret), (long) secret.length, seg(recipientKemPublic), (long) recipientKemPublic.length,
                seg(senderSigSecret), (long) senderSigSecret.length)); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }
    public PortableAccount openAccountSignedFrom(byte[] envelope, byte[] recipientKemSecret, byte[] senderSigPublic) {
        try { return splitBlob(consume((MemorySegment) pOpenSigned.invoke(arena.allocate(RES),
                seg(envelope), (long) envelope.length, seg(recipientKemSecret), (long) recipientKemSecret.length,
                seg(senderSigPublic), (long) senderSigPublic.length))); }
        catch (Throwable t) { throw new RuntimeException(t); }
    }

    // -- Threshold (FROST t-of-n) accounts --------------------------------------

    /** A participant's share of a threshold account (opaque, self-describing). */
    public record ShareCard(byte[] bytes, int index, byte[] groupPublicKey, byte[] publicShare) {}

    /** Deal a t-of-n account: n share cards, any t of which can sign. */
    public ShareCard[] createThresholdAccount(int n, int t) {
        byte[] blob = consume((MemorySegment) invokeShort2(thCreate, (short) n, (short) t));
        int count = ((blob[0] & 0xFF) << 8) | (blob[1] & 0xFF);
        ShareCard[] cards = new ShareCard[count];
        int off = 2;
        for (int i = 0; i < count; i++) {
            int len = ((blob[off] & 0xFF) << 24) | ((blob[off+1] & 0xFF) << 16) | ((blob[off+2] & 0xFF) << 8) | (blob[off+3] & 0xFF);
            off += 4;
            byte[] card = java.util.Arrays.copyOfRange(blob, off, off + len);
            off += len;
            byte[] info = consume((MemorySegment) invokeResA(thInfo, card));
            int idx = ((info[0] & 0xFF) << 8) | (info[1] & 0xFF);
            cards[i] = new ShareCard(card, idx, java.util.Arrays.copyOfRange(info, 2, 34),
                                     java.util.Arrays.copyOfRange(info, 34, 66));
        }
        return cards;
    }

    /** Seal a share card for rest (ops=mem=0 -> SENSITIVE preset). */
    public byte[] sealShare(byte[] card, String passphrase, long ops, long mem) {
        try { return consume((MemorySegment) thSeal.invoke(arena.allocate(RES), seg(card), (long) card.length,
                arena.allocateFrom(passphrase), ops, mem)); }
        catch (Throwable e) { throw new RuntimeException(e); }
    }
    public byte[] unsealShare(byte[] envelope, String passphrase) {
        try { return consume((MemorySegment) thUnseal.invoke(arena.allocate(RES), seg(envelope), (long) envelope.length,
                arena.allocateFrom(passphrase))); }
        catch (Throwable e) { throw new RuntimeException(e); }
    }

    /** Round 1 for a card: [nonces 64][commitment 66]. Keep nonces; broadcast commitment. */
    public byte[] commit(byte[] card) {
        try { return consume((MemorySegment) thCommit.invoke(arena.allocate(RES), seg(card), (long) card.length)); }
        catch (Throwable e) { throw new RuntimeException(e); }
    }

    /** A two-round signing ceremony over a fixed message + roster. */
    public final class Ceremony implements AutoCloseable {
        private final MemorySegment h;
        Ceremony(byte[] groupPK, byte[] message, byte[][] commitments) {
            int total = 0; for (byte[] c : commitments) total += c.length;
            byte[] roster = new byte[total]; int o = 0;
            for (byte[] c : commitments) { System.arraycopy(c, 0, roster, o, c.length); o += c.length; }
            try {
                h = (MemorySegment) thCerNew.invoke(seg(groupPK), seg(message), (long) message.length, seg(roster), (long) roster.length);
            } catch (Throwable e) { throw new RuntimeException(e); }
            if (h.address() == 0) throw new IllegalStateException("threshold ceremony creation failed");
        }
        /** Round 2 for a card using its round-1 nonces: a 32-byte share. */
        public byte[] sign(byte[] card, byte[] nonces) {
            try { return consume((MemorySegment) thSign.invoke(arena.allocate(RES), h, seg(card), (long) card.length, seg(nonces))); }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** Attribute a share to participant [id] (public share 32 B). */
        public boolean verifyShare(int id, byte[] publicShare, byte[] sigShare) {
            try { return (int) thVerify.invoke(h, (short) id, seg(publicShare), seg(sigShare)) == 1; }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** Aggregate (id -> 32-byte share) pairs into a 64-byte Ed25519 signature. */
        public byte[] aggregate(int[] ids, byte[][] sigShares) {
            MemorySegment idSeg = arena.allocate(JAVA_SHORT, ids.length);
            for (int i = 0; i < ids.length; i++) idSeg.setAtIndex(JAVA_SHORT, i, (short) ids[i]);
            byte[] flat = new byte[sigShares.length * 32]; int o = 0;
            for (byte[] s : sigShares) { System.arraycopy(s, 0, flat, o, 32); o += 32; }
            try { return consume((MemorySegment) thAgg.invoke(arena.allocate(RES), h, idSeg, seg(flat), (long) ids.length)); }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        @Override public void close() { try { thCerFree.invoke(h); } catch (Throwable ignored) {} }
    }
    public Ceremony newCeremony(byte[] groupPK, byte[] message, byte[][] commitments) {
        return new Ceremony(groupPK, message, commitments);
    }

    private Object invokeShort2(MethodHandle mh, short a, short b) {
        try { return mh.invoke(arena.allocate(RES), a, b); } catch (Throwable e) { throw new RuntimeException(e); }
    }
    private Object invokeResA(MethodHandle mh, byte[] a) {
        try { return mh.invoke(arena.allocate(RES), seg(a), (long) a.length); } catch (Throwable e) { throw new RuntimeException(e); }
    }

    // -- AssuranceProfile + SealedMempool ---------------------------------------

    /** The tiers a security posture mandates at every seam. */
    public record AssuranceProfile(boolean ratcheted, boolean vrf, boolean thresholdCustody,
                                   boolean sealedMempool, byte[] sigAlgos) {}

    /** Describe a posture: 1 = standard, 2 = hardened, 3 = fortress. */
    public AssuranceProfile describeAssurance(int level) {
        byte[] b;
        try { b = consume((MemorySegment) asDescribe.invoke(arena.allocate(RES), (short) level)); }
        catch (Throwable e) { throw new RuntimeException(e); }
        int n = b[4] & 0xFF;
        return new AssuranceProfile(b[0] == 1, b[1] == 1, b[2] == 1, b[3] == 1,
                                    java.util.Arrays.copyOfRange(b, 5, 5 + n));
    }

    /** Encode a transaction from its fields (e.g. to seal into a SealedMempool). */
    public byte[] encodeTx(int version, long chainId, int sigAlgo, long fee, byte[] body, byte[] author, byte[] signature) {
        try { return consume((MemorySegment) txEncode.invoke(arena.allocate(RES), (byte) version, chainId, (byte) sigAlgo, fee,
                seg(body), (long) body.length, seg(author), (long) author.length, seg(signature), (long) signature.length)); }
        catch (Throwable e) { throw new RuntimeException(e); }
    }
    /** Seal an encoded transaction to the proposer's hybrid-KEM public key. */
    public byte[] sealTx(byte[] txEncoded, byte[] proposerKemPublic) {
        try { return consume((MemorySegment) smSeal.invoke(arena.allocate(RES), seg(txEncoded), (long) txEncoded.length,
                seg(proposerKemPublic), (long) proposerKemPublic.length)); }
        catch (Throwable e) { throw new RuntimeException(e); }
    }

    /** An encrypted mempool: submissions stay sealed to the proposer until reveal. */
    public final class SealedMempool implements AutoCloseable {
        private final MemorySegment h;
        SealedMempool(long capacity) {
            try { h = (MemorySegment) smNew.invoke(capacity); } catch (Throwable e) { throw new RuntimeException(e); }
        }
        public boolean add(byte[] sealedTx) {
            try { return (int) smAdd.invoke(h, seg(sealedTx), (long) sealedTx.length) == 1; }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        public long size() { try { return (long) smSize.invoke(h); } catch (Throwable e) { throw new RuntimeException(e); } }
        public byte[][] reveal(byte[] proposerKemSecret, long max) {
            byte[] blob;
            try { blob = consume((MemorySegment) smReveal.invoke(arena.allocate(RES), h, seg(proposerKemSecret), (long) proposerKemSecret.length, max)); }
            catch (Throwable e) { throw new RuntimeException(e); }
            int count = ((blob[0] & 0xFF) << 24) | ((blob[1] & 0xFF) << 16) | ((blob[2] & 0xFF) << 8) | (blob[3] & 0xFF);
            byte[][] txs = new byte[count][]; int off = 4;
            for (int i = 0; i < count; i++) {
                int len = ((blob[off] & 0xFF) << 24) | ((blob[off+1] & 0xFF) << 16) | ((blob[off+2] & 0xFF) << 8) | (blob[off+3] & 0xFF);
                off += 4; txs[i] = java.util.Arrays.copyOfRange(blob, off, off + len); off += len;
            }
            return txs;
        }
        @Override public void close() { try { smFree.invoke(h); } catch (Throwable ignored) {} }
    }
    public SealedMempool newSealedMempool(long capacity) { return new SealedMempool(capacity); }

    // -- CredentialGate (anonymous membership admission, Option A) ---------------

    /** A BBS issuer keypair (public 96 B, secret 32 B) - the chain's issuer identity. */
    public PortableAccount newBBSIssuer(byte[] keyMaterial) {
        try { return splitBlob(consume((MemorySegment) bbsKeygen.invoke(arena.allocate(RES), seg(keyMaterial), (long) keyMaterial.length))); }
        catch (Throwable e) { throw new RuntimeException(e); }
    }

    /** A membership policy. Its canonical {@code encoded} bytes drive the gate; its
     *  {@code hash()} is meant to be committed into genesis (like an AssuranceProfile). */
    public final class CredentialPolicy {
        final byte[] encoded;
        public CredentialPolicy(long chainId, byte[] issuerPublic, byte[] header, int attributeCount, int minTier) {
            this(chainId, issuerPublic, header, attributeCount, minTier, 0);
        }
        /** mode: 0 = unlinkable (Option A), 1 = pseudonymous (Option B). */
        public CredentialPolicy(long chainId, byte[] issuerPublic, byte[] header, int attributeCount, int minTier, int mode) {
            try {
                encoded = mode == 0
                    ? consume((MemorySegment) credPolicyEncode.invoke(arena.allocate(RES), chainId,
                        seg(issuerPublic), (long) issuerPublic.length, seg(header), (long) header.length, attributeCount, (byte) minTier))
                    : consume((MemorySegment) credPolicyEncodeEx.invoke(arena.allocate(RES), chainId,
                        seg(issuerPublic), (long) issuerPublic.length, seg(header), (long) header.length, attributeCount, (byte) minTier, (byte) mode));
            } catch (Throwable e) { throw new RuntimeException(e); }
        }
        public byte[] encoded() { return encoded; }
        /** The 32-byte commitment to commit into genesis. */
        public byte[] hash() {
            try { return consume((MemorySegment) credPolicyHash.invoke(arena.allocate(RES), seg(encoded), (long) encoded.length)); }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** Issuer side: assemble + BBS-sign a credential (holder-secret blob). */
        public byte[] issue(byte[] issuerSecret, byte[] memberSecret, int tier, long notAfter, int issuerEpoch) {
            try { return consume((MemorySegment) credIssue.invoke(arena.allocate(RES), seg(encoded), (long) encoded.length,
                    seg(issuerSecret), (long) issuerSecret.length, seg(memberSecret), (long) memberSecret.length,
                    (byte) tier, notAfter, issuerEpoch)); }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** Holder side: an unlinkable presentation for (epoch, scope, challenge). */
        public byte[] present(byte[] credential, int epoch, String scope, byte[] challenge) {
            try { return consume((MemorySegment) credPresent.invoke(arena.allocate(RES), seg(encoded), (long) encoded.length,
                    seg(credential), (long) credential.length, epoch, arena.allocateFrom(scope), seg(challenge), (long) challenge.length)); }
            catch (Throwable e) { throw new RuntimeException(e); }
        }

        // -- Option B - pseudonymous --------------------------------------------
        /** Holder step 1: derive a nym seed and blind-commit it (holder-secret blob). */
        public byte[] membershipCommit(byte[] memberSecret) {
            try { return consume((MemorySegment) membershipCommit.invoke(arena.allocate(RES), seg(encoded), (long) encoded.length,
                    seg(memberSecret), (long) memberSecret.length)); }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** Issuer step 2: blind-sign the holder's commitment (never sees the seed). */
        public byte[] blindIssue(byte[] issuerSecret, byte[] commitment, int tier, long notAfter, int issuerEpoch) {
            try { return consume((MemorySegment) blindIssue.invoke(arena.allocate(RES), seg(encoded), (long) encoded.length,
                    seg(issuerSecret), (long) issuerSecret.length, seg(commitment), (long) commitment.length, (byte) tier, notAfter, issuerEpoch)); }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** Holder step 3: a per-epoch pseudonymous presentation. */
        public byte[] presentPseudonymous(byte[] credential, int epoch, String scope, byte[] challenge) {
            try { return consume((MemorySegment) credPresentPseud.invoke(arena.allocate(RES), seg(encoded), (long) encoded.length,
                    seg(credential), (long) credential.length, epoch, arena.allocateFrom(scope), seg(challenge), (long) challenge.length)); }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
    }

    /** The commitment is the first length-prefixed field of a membership-commit blob. */
    public static byte[] extractCommitment(byte[] membershipCommit) {
        int n = ((membershipCommit[0] & 0xFF) << 24) | ((membershipCommit[1] & 0xFF) << 16)
              | ((membershipCommit[2] & 0xFF) << 8) | (membershipCommit[3] & 0xFF);
        return java.util.Arrays.copyOfRange(membershipCommit, 4, 4 + n);
    }
    /** Holder step 3 (assemble): finalize the pseudonymous credential (holder-secret). */
    public byte[] finalizeMembership(byte[] membershipCommit, byte[] blindIssuance) {
        try { return consume((MemorySegment) finalizeMembership.invoke(arena.allocate(RES),
                seg(membershipCommit), (long) membershipCommit.length, seg(blindIssuance), (long) blindIssuance.length)); }
        catch (Throwable e) { throw new RuntimeException(e); }
    }

    /** A verifier's admission gate (holds the issuer-epoch deny-list). */
    public final class CredentialGate implements AutoCloseable {
        private final MemorySegment h;
        CredentialGate(CredentialPolicy policy) {
            try { h = (MemorySegment) credGateNew.invoke(seg(policy.encoded), (long) policy.encoded.length); }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** Bulk revocation: refuse every credential issued in this epoch. */
        public void revokeIssuerEpoch(int epoch) {
            try { credGateRevoke.invoke(h, epoch); } catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** Admit? Runs the full checklist. Total - a malformed presentation is a quiet false. */
        public boolean admit(int epoch, String scope, byte[] challenge, byte[] presentation, long now) {
            try { return (int) credGateAdmit.invoke(h, epoch, arena.allocateFrom(scope), seg(challenge), (long) challenge.length,
                    seg(presentation), (long) presentation.length, now) == 1; }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** Option B: admit? Returns the 48-byte pseudonym on success, or null on reject. */
        public byte[] admitPseudonymous(int epoch, String scope, byte[] challenge, byte[] presentation, long now) {
            try {
                byte[] nym = consume((MemorySegment) credGateAdmitPseud.invoke(arena.allocate(RES), h, epoch, arena.allocateFrom(scope),
                        seg(challenge), (long) challenge.length, seg(presentation), (long) presentation.length, now));
                return nym.length == 0 ? null : nym;
            } catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** section 6.2 targeted revocation of an exact pseudonym. */
        public void revokeNym(byte[] nym) {
            try { credGateRevokeNym.invoke(h, seg(nym), (long) nym.length); } catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** section 6.3 Sybil cap: record an admitted nym; true if first this (epoch, scope). */
        public boolean noteAdmission(int epoch, String scope, byte[] nym) {
            try { return (int) credGateNoteAdmission.invoke(h, epoch, arena.allocateFrom(scope), seg(nym), (long) nym.length) == 1; }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        @Override public void close() { try { credGateFree.invoke(h); } catch (Throwable ignored) {} }
    }
    public CredentialGate newCredentialGate(CredentialPolicy policy) { return new CredentialGate(policy); }

    // -- Composable chains (blueprint + registry discovery + assembly) -----------

    /** A validator BLS keypair (public 48 B, secret 32 B) from >= 32 B of key material. */
    public PortableAccount newBLSValidator(byte[] ikm) {
        try { return splitBlob(consume((MemorySegment) blsKeygen.invoke(arena.allocate(RES), seg(ikm), (long) ikm.length))); }
        catch (Throwable e) { throw new RuntimeException(e); }
    }

    /** A declared mixture: engine / state model / proposer ids + component params. */
    public final class Blueprint {
        final byte[] encoded;
        public Blueprint(String consensus, String stateModel, String proposer, byte[] params) {
            byte[] p = params == null ? new byte[0] : params;
            try { encoded = consume((MemorySegment) bpEncode.invoke(arena.allocate(RES), arena.allocateFrom(consensus),
                    arena.allocateFrom(stateModel), arena.allocateFrom(proposer == null ? "" : proposer), seg(p), (long) p.length)); }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        public byte[] encoded() { return encoded; }
        public byte[] hash() {
            try { return consume((MemorySegment) bpHash.invoke(arena.allocate(RES), seg(encoded), (long) encoded.length)); }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
    }

    private java.util.List<String> registryIds(int kind) {
        byte[] b;
        try { b = consume((MemorySegment) registryIds.invoke(arena.allocate(RES), (byte) kind)); }
        catch (Throwable e) { throw new RuntimeException(e); }
        int n = ((b[0] & 0xFF) << 24) | ((b[1] & 0xFF) << 16) | ((b[2] & 0xFF) << 8) | (b[3] & 0xFF);
        var out = new java.util.ArrayList<String>(n); int off = 4;
        for (int i = 0; i < n; i++) {
            int l = ((b[off] & 0xFF) << 24) | ((b[off+1] & 0xFF) << 16) | ((b[off+2] & 0xFF) << 8) | (b[off+3] & 0xFF); off += 4;
            out.add(new String(b, off, l, java.nio.charset.StandardCharsets.UTF_8)); off += l;
        }
        return out;
    }
    public java.util.List<String> engineIds() { return registryIds(0); }
    public java.util.List<String> stateModelIds() { return registryIds(1); }
    public java.util.List<String> proposerIds() { return registryIds(2); }

    /** A minimal genesis: chain params + posture (1/2/3) + seeded state (32-byte addrs) + validators (48-byte BLS keys). */
    public byte[] buildGenesis(long chainId, long timestamp, int assuranceLevel, int stateModel,
                               byte[][] addrs, long[] balances, byte[][] validatorPks, long[] powers) {
        byte[] a = new byte[32 * addrs.length]; for (int i = 0; i < addrs.length; i++) System.arraycopy(addrs[i], 0, a, 32 * i, 32);
        byte[] k = new byte[48 * validatorPks.length]; for (int i = 0; i < validatorPks.length; i++) System.arraycopy(validatorPks[i], 0, k, 48 * i, 48);
        try { return consume((MemorySegment) genesisBuild.invoke(arena.allocate(RES), chainId, timestamp, (short) assuranceLevel, (byte) stateModel,
                seg(a), arena.allocateFrom(JAVA_LONG, balances), (long) addrs.length,
                seg(k), arena.allocateFrom(JAVA_LONG, powers), (long) validatorPks.length)); }
        catch (Throwable e) { throw new RuntimeException(e); }
    }
    /** Commit a mixture into a genesis (its hash changes: the mixture is chain identity). */
    public byte[] commitBlueprint(byte[] genesis, Blueprint b) {
        try { return consume((MemorySegment) genesisCommitBp.invoke(arena.allocate(RES), seg(genesis), (long) genesis.length, seg(b.encoded), (long) b.encoded.length)); }
        catch (Throwable e) { throw new RuntimeException(e); }
    }
    public byte[] genesisHash(byte[] genesis) {
        try { return consume((MemorySegment) genesisHash.invoke(arena.allocate(RES), seg(genesis), (long) genesis.length)); }
        catch (Throwable e) { throw new RuntimeException(e); }
    }

    /** An assembled node behind the uniform propose -> (seal | mine) -> submit flow. */
    public final class Chain implements AutoCloseable {
        private final MemorySegment h;
        Chain(MemorySegment h) { this.h = h; }
        public long height() { try { return (long) chainHeight.invoke(h); } catch (Throwable e) { throw new RuntimeException(e); } }
        public byte[] blueprint() { try { return consume((MemorySegment) chainBlueprint.invoke(arena.allocate(RES), h)); } catch (Throwable e) { throw new RuntimeException(e); } }
        public byte[] propose(long timestamp) { try { return consume((MemorySegment) chainPropose.invoke(arena.allocate(RES), h, timestamp)); } catch (Throwable e) { throw new RuntimeException(e); } }
        /** PoS / VRF justification; empty for engines that do not seal. */
        public byte[] seal(byte[] block) { try { return consume((MemorySegment) chainSeal.invoke(arena.allocate(RES), h, seg(block), (long) block.length)); } catch (Throwable e) { throw new RuntimeException(e); } }
        /** PoW: the solved block. */
        public byte[] mine(byte[] block) { try { return consume((MemorySegment) chainMine.invoke(arena.allocate(RES), h, seg(block), (long) block.length)); } catch (Throwable e) { throw new RuntimeException(e); } }
        public boolean submit(byte[] block, byte[] justification) {
            byte[] j = justification == null ? new byte[0] : justification;
            try { return (int) chainSubmit.invoke(h, seg(block), (long) block.length, seg(j), (long) j.length) == 1; }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        private long eventHandle;
        /** Buffered block events (block_finalized or block_rejected per submit) as a JSON array; empty while a hook is set. */
        public String drainEvents() { return new String(consume(invokeRes(chainDrainEvents, h)), StandardCharsets.UTF_8); }
        /** Receives every block event as one JSON object, synchronously from the submit that caused it; null removes the hook. */
        public void setEventHook(java.util.function.Consumer<String> hook) { eventHandle = hookEvents(chainSetEventHook, h, eventHandle, hook); }
        /** Prometheus text: chain height, blocks finalized and rejected, transactions applied. */
        public String metrics() { return new String(consume(invokeRes(chainMetrics, h)), StandardCharsets.UTF_8); }
        @Override public void close() {
            eventHandle = hookEvents(chainSetEventHook, h, eventHandle, null);
            try { chainFree.invoke(h); } catch (Throwable ignored) {}
        }

        // -- Subchains --
        /** Propose the next block on a PARENT chain, anchoring encoded anchors (ascending child id). Throws on an invalid anchor. */
        public byte[] proposeAnchored(long timestamp, byte[][] anchors) {
            byte[] blob = blobList(anchors);
            try { return consume((MemorySegment) chainProposeAnchored.invoke(arena.allocate(RES), h, timestamp, seg(blob), (long) blob.length)); } catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** BFT: this validator's precommit aggregated into a commit certificate (the justification for submit; what a parent anchors). */
        public byte[] certify(byte[] block) { try { return consume((MemorySegment) chainCertify.invoke(arena.allocate(RES), h, seg(block), (long) block.length)); } catch (Throwable e) { throw new RuntimeException(e); } }
        /** BFT: schedule a coordinated validator-set change (48-byte keys + powers); the next proposal announces it. */
        public boolean scheduleValidators(byte[][] pks, long[] powers) {
            byte[] k = new byte[48 * pks.length];
            for (int i = 0; i < pks.length; i++) System.arraycopy(pks[i], 0, k, 48 * i, 48);
            try { return (int) chainScheduleValidators.invoke(h, seg(k), arena.allocateFrom(JAVA_LONG, powers), (long) pks.length) == 1; }
            catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** BFT: the validator set in force for the next block. */
        public java.util.List<ValidatorEntry> validators() {
            try { return decodeValidatorSet(consume((MemorySegment) chainValidatorsH.invoke(arena.allocate(RES), h))); } catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** A parent's last anchored header for a child. */
        public byte[] childTip(long childChainId) { try { return consume((MemorySegment) chainChildTip.invoke(arena.allocate(RES), h, childChainId)); } catch (Throwable e) { throw new RuntimeException(e); } }
        /** A proof hop from this parent's current registry into a child. */
        public byte[] subchainHop(long childChainId) { try { return consume((MemorySegment) chainSubchainHop.invoke(arena.allocate(RES), h, childChainId)); } catch (Throwable e) { throw new RuntimeException(e); } }
        /** State-inclusion proof for a 32-byte address on this account-model chain. */
        public byte[] proveAccount(byte[] address) { try { return consume((MemorySegment) chainProveAccount.invoke(arena.allocate(RES), h, seg(address))); } catch (Throwable e) { throw new RuntimeException(e); } }
    }

    /** The canonical ValidatorSet encoding: [count]([len][pk][power u64])x. */
    static java.util.List<ValidatorEntry> decodeValidatorSet(byte[] b) {
        java.nio.ByteBuffer bb = java.nio.ByteBuffer.wrap(b);
        int n = bb.getInt(); var out = new java.util.ArrayList<ValidatorEntry>(n);
        for (int i = 0; i < n; i++) { byte[] pk = new byte[bb.getInt()]; bb.get(pk); out.add(new ValidatorEntry(pk, bb.getLong())); }
        return out;
    }
    static byte[] encodeValidatorSet(java.util.List<ValidatorEntry> vs) {
        int n = 4; for (var v : vs) n += 4 + v.publicKey().length + 8;
        java.nio.ByteBuffer bb = java.nio.ByteBuffer.allocate(n).putInt(vs.size());
        for (var v : vs) bb.putInt(v.publicKey().length).put(v.publicKey()).putLong(v.power());
        return bb.array();
    }
    /** A header, its certificate, and the set in force at its height. */
    public record Checkpoint(byte[] header, byte[] certificate, java.util.List<ValidatorEntry> validators) {}
    public static final int SKIP_REFUSED = 0, SKIP_ACCEPTED = 1, SKIP_INSUFFICIENT_OVERLAP = 2;

    /** A light client: follows a header chain with certificates only. Sequential accept plus the
     *  OVERLAP (skipping) rule - acceptSkipping jumps to a far header when its own set certifies it
     *  and the signers hold more than the trust level (default 1/3) of the set the client already
     *  trusts; syncTo bisects when the overlap is too thin. */
    public final class LightClient implements AutoCloseable {
        private final MemorySegment h;
        LightClient(long chainId, java.util.List<ValidatorEntry> validators, byte[] trustedHeader) {
            byte[] vs = encodeValidatorSet(validators);
            try { h = (MemorySegment) lcNew.invoke(chainId, seg(vs), (long) vs.length, seg(trustedHeader), (long) trustedHeader.length); } catch (Throwable e) { throw new RuntimeException(e); }
            if (h.address() == 0) throw new IllegalArgumentException("light client: malformed validator set or header");
        }
        public long height() { try { return (long) lcHeight.invoke(h); } catch (Throwable e) { throw new RuntimeException(e); } }
        public byte[] tip() { try { return consume((MemorySegment) lcTip.invoke(arena.allocate(RES), h)); } catch (Throwable e) { throw new RuntimeException(e); } }
        public java.util.List<ValidatorEntry> validators() { try { return decodeValidatorSet(consume((MemorySegment) lcValidators.invoke(arena.allocate(RES), h))); } catch (Throwable e) { throw new RuntimeException(e); } }
        public void setTrustLevel(int numerator, int denominator) { try { lcSetTrust.invoke(h, numerator, denominator); } catch (Throwable e) { throw new RuntimeException(e); } }
        public void setMaxSkip(long blocks) { try { lcSetMaxSkip.invoke(h, blocks); } catch (Throwable e) { throw new RuntimeException(e); } }
        public boolean accept(byte[] header, byte[] certificate) {
            try { return (int) lcAccept.invoke(h, seg(header), (long) header.length, seg(certificate), (long) certificate.length) == 1; } catch (Throwable e) { throw new RuntimeException(e); }
        }
        public int acceptSkipping(Checkpoint cp) {
            byte[] vs = encodeValidatorSet(cp.validators());
            try { return (int) lcAcceptSkipping.invoke(h, seg(cp.header()), (long) cp.header().length, seg(cp.certificate()), (long) cp.certificate().length, seg(vs), (long) vs.length); } catch (Throwable e) { throw new RuntimeException(e); }
        }
        /** Bisection: fetch(height) -> checkpoint or null. Throws on a hard refusal / missing checkpoint. */
        public void syncTo(Checkpoint target, long targetHeight, java.util.function.LongFunction<Checkpoint> fetch) {
            var stack = new java.util.ArrayDeque<Object[]>(); stack.push(new Object[]{target, targetHeight});
            for (int n = 0; !stack.isEmpty(); n++) {
                if (n > 128) throw new IllegalStateException("light client: bisection exceeded its fetch budget");
                Object[] top = stack.peek(); Checkpoint cp = (Checkpoint) top[0]; long hi = (Long) top[1];
                int r = acceptSkipping(cp);
                if (r == SKIP_ACCEPTED) { stack.pop(); continue; }
                if (r != SKIP_INSUFFICIENT_OVERLAP) throw new IllegalStateException("light client: checkpoint refused");
                long lo = height();
                if (hi - lo <= 1) throw new IllegalStateException("light client: insufficient overlap and no intermediate header");
                long mid = lo + (hi - lo) / 2;
                Checkpoint m = fetch.apply(mid);
                if (m == null) throw new IllegalStateException("light client: checkpoint unavailable at " + mid);
                stack.push(new Object[]{m, mid});
            }
        }
        @Override public void close() { try { lcFree.invoke(h); } catch (Throwable ignored) {} }
    }
    public LightClient newLightClient(long chainId, java.util.List<ValidatorEntry> validators, byte[] trustedHeader) { return new LightClient(chainId, validators, trustedHeader); }

    // -- Chain tree: declare a FAMILY of chains and drive it together ----------
    /** One chain of a family; children are subchains (must be BFT). secret = null for a validator this process does not hold. */
    public static final class TreeNode {
        public long chainId; public Blueprint blueprint; public int assuranceLevel = 1;
        public byte[][] accountAddrs = new byte[0][]; public long[] balances = new long[0];
        public byte[][] validatorPks = new byte[0][]; public byte[][] validatorSecrets = null; public long[] powers = new long[0];
        public int maxAnchorsPerBlock = 0, maxHeadersPerAnchor = 0;
        public java.util.List<TreeNode> children = new java.util.ArrayList<>();
        public TreeNode(long chainId) { this.chainId = chainId; }
        public TreeNode validators(byte[][] pks, byte[][] secrets, long[] pw) { validatorPks = pks; validatorSecrets = secrets; powers = pw; return this; }
        public TreeNode accounts(byte[][] addrs, long[] bal) { accountAddrs = addrs; balances = bal; return this; }
        public TreeNode child(TreeNode c) { children.add(c); return this; }
    }
    public byte[] encodeTreeNode(TreeNode n) {
        byte[] bp = (n.blueprint == null ? new Blueprint("bft", "account", "", null) : n.blueprint).encoded();
        byte[] addrs = new byte[32 * n.accountAddrs.length]; for (int i = 0; i < n.accountAddrs.length; i++) System.arraycopy(n.accountAddrs[i], 0, addrs, 32 * i, 32);
        byte[] pks = new byte[48 * n.validatorPks.length]; for (int i = 0; i < n.validatorPks.length; i++) System.arraycopy(n.validatorPks[i], 0, pks, 48 * i, 48);
        byte[] secrets = new byte[32 * n.validatorPks.length];
        if (n.validatorSecrets != null) for (int i = 0; i < n.validatorPks.length; i++) if (n.validatorSecrets[i] != null) System.arraycopy(n.validatorSecrets[i], 0, secrets, 32 * i, 32);
        byte[][] kids = new byte[n.children.size()][]; for (int i = 0; i < kids.length; i++) kids[i] = encodeTreeNode(n.children.get(i));
        byte[] blob = blobList(kids);
        return call(treeNodeEncode, n.chainId, seg(bp), (long) bp.length, (short) n.assuranceLevel, seg(addrs), arena.allocateFrom(JAVA_LONG, n.balances), (long) n.accountAddrs.length,
                seg(pks), seg(secrets), arena.allocateFrom(JAVA_LONG, n.powers), (long) n.validatorPks.length, (short) n.maxAnchorsPerBlock, (short) n.maxHeadersPerAnchor, seg(blob), (long) blob.length);
    }
    /** Why a spec is structurally invalid ("" = valid). */
    public String chainTreeSpecError(byte[] spec) { return new String(call(treeSpecError, seg(spec), (long) spec.length)); }

    private static final java.util.Map<Long, ChainTree> TREES = new java.util.concurrent.ConcurrentHashMap<>();
    @SuppressWarnings("unused")
    private static void treeFinalizedUpcall(MemorySegment ctx, long chainId, MemorySegment block, long len) {
        ChainTree t = TREES.get(ctx.address()); if (t != null && t.onFinalized != null) t.onFinalized.accept(chainId, block.reinterpret(len).toArray(JAVA_BYTE));
    }
    @SuppressWarnings("unused")
    private static int treeTxSourceUpcall(MemorySegment ctx, long chainId, long ts, MemorySegment out, long cap, MemorySegment outLen) {
        ChainTree t = TREES.get(ctx.address()); if (t == null || t.txSource == null) return 0;
        byte[][] txs = t.txSource.apply(chainId, ts); byte[] blob = blobList(txs == null ? new byte[0][] : txs);
        if (blob.length > cap) return 0;
        MemorySegment.copy(blob, 0, out.reinterpret(cap), JAVA_BYTE, 0, blob.length); outLen.reinterpret(8).set(JAVA_LONG, 0, (long) blob.length); return 1;
    }
    /** A built family driven in-process: produce() anchors children upward; proveAccount() proves any descendant from the root. */
    public final class ChainTree implements AutoCloseable {
        private final MemorySegment h; private final long handle;
        public java.util.function.BiConsumer<Long, byte[]> onFinalized;
        public java.util.function.BiFunction<Long, Long, byte[][]> txSource;
        ChainTree(TreeNode root) {
            byte[] spec = encodeTreeNode(root);
            String err = chainTreeSpecError(spec);
            if (!err.isEmpty()) throw new IllegalArgumentException("chain tree: " + err);
            try { h = (MemorySegment) treeBuild.invoke(seg(spec), (long) spec.length); } catch (Throwable e) { throw new RuntimeException(e); }
            if (h.address() == 0) throw new IllegalArgumentException("chain tree: build rejected (unknown mixture or posture-forbidden engine)");
            handle = NEXT_HANDLE.getAndIncrement(); TREES.put(handle, this);
            MemorySegment fin = upcall("treeFinalizedUpcall", java.lang.invoke.MethodType.methodType(void.class, MemorySegment.class, long.class, MemorySegment.class, long.class),
                    FunctionDescriptor.ofVoid(ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
            MemorySegment tx = upcall("treeTxSourceUpcall", java.lang.invoke.MethodType.methodType(int.class, MemorySegment.class, long.class, long.class, MemorySegment.class, long.class, MemorySegment.class),
                    FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_LONG, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS));
            try { treeSetFinalized.invoke(h, fin, MemorySegment.ofAddress(handle)); treeSetTxSource.invoke(h, tx, MemorySegment.ofAddress(handle)); } catch (Throwable e) { throw new RuntimeException(e); }
        }
        public long[] chainIds() { byte[] b = call(treeChainIds, h); java.nio.ByteBuffer bb = java.nio.ByteBuffer.wrap(b); int n = bb.getInt(); long[] out = new long[n]; for (int i = 0; i < n; i++) out[i] = bb.getLong(); return out; }
        public long height(long chainId) { try { return (long) treeHeight.invoke(h, chainId); } catch (Throwable e) { throw new RuntimeException(e); } }
        public byte[] genesis(long chainId) { return call(treeGenesis, h, chainId); }
        public byte[] tip(long chainId) { return call(treeTip, h, chainId); }
        public int pendingAnchors(long chainId) { try { return (int) treePending.invoke(h, chainId); } catch (Throwable e) { throw new RuntimeException(e); } }
        public byte[] produce(long chainId, long timestamp) { return call(treeProduce, h, chainId, timestamp); }
        public byte[] proveAccount(long chainId, byte[] address) { return call(treeProve, h, chainId, seg(address)); }
        public boolean verify(byte[] proof) { try { return (int) treeVerify.invoke(h, seg(proof), (long) proof.length) == 1; } catch (Throwable e) { throw new RuntimeException(e); } }
        @Override public void close() { try { treeFree.invoke(h); } catch (Throwable ignored) {} TREES.remove(handle); }
    }
    public ChainTree buildChainTree(TreeNode root) { return new ChainTree(root); }

    // -- Subchains: a parent block anchors a child chain's finalized headers ----
    // (docs/design/SUBCHAINS.md). Register a child at the parent's genesis by its
    // genesis HEADER + fixed validator set; anchor its certified headers; prove a
    // state entry on a descendant from the parent's header. v1 anchors BFT children.

    /** [count u32]([len u32][bytes])x - the C ABI's blob-list convention. */
    static byte[] blobList(byte[][] items) {
        int n = 4; for (byte[] it : items) n += 4 + it.length;
        java.nio.ByteBuffer b = java.nio.ByteBuffer.allocate(n).putInt(items.length);
        for (byte[] it : items) b.putInt(it.length).put(it);
        return b.array();
    }
    private byte[] call(MethodHandle mh, Object... args) {
        Object[] all = new Object[args.length + 1];
        all[0] = arena.allocate(RES); System.arraycopy(args, 0, all, 1, args.length);
        try { return consume((MemorySegment) mh.invokeWithArguments(all)); } catch (Throwable e) { throw new RuntimeException(e); }
    }
    /** A genesis's genesis HEADER - the identity a parent registers a child by. */
    public byte[] genesisHeader(byte[] genesis) { return call(genesisHeaderH, seg(genesis), (long) genesis.length); }
    /** Encode a child registration (48-byte BLS keys; maxDepth = the child's own subtree depth, 0 = leaf). */
    public byte[] encodeChildRegistration(long chainId, byte[] genesisHeader, byte[][] validatorPks, long[] powers, int maxDepth) {
        byte[] k = new byte[48 * validatorPks.length];
        for (int i = 0; i < validatorPks.length; i++) System.arraycopy(validatorPks[i], 0, k, 48 * i, 48);
        return call(childRegEncode, chainId, seg(genesisHeader), (long) genesisHeader.length, seg(k), arena.allocateFrom(JAVA_LONG, powers), (long) validatorPks.length, (byte) maxDepth);
    }
    /** Encode a parent's subchain policy from encoded registrations (ascending, unique chain ids). 0 = unlimited. */
    public byte[] encodeSubchainPolicy(int maxChildren, int maxDepth, int maxAnchorsPerBlock, int maxHeadersPerAnchor, byte[][] registrations) {
        byte[] blob = blobList(registrations);
        return call(subchainPolicyEncode, (short) maxChildren, (byte) maxDepth, (short) maxAnchorsPerBlock, (short) maxHeadersPerAnchor, seg(blob), (long) blob.length);
    }
    /** Commit a policy into a genesis (this chain becomes a PARENT; its hash changes). Throws on an invalid policy. */
    public byte[] commitSubchainPolicy(byte[] genesis, byte[] policy) { return call(genesisCommitSubchains, seg(genesis), (long) genesis.length, seg(policy), (long) policy.length); }
    /** Bind a child genesis to its parent (id + 32-byte parent genesis hash); the child can never be re-parented. */
    public byte[] commitParentBinding(byte[] genesis, long parentChainId, byte[] parentGenesisHash) { return call(genesisCommitBinding, seg(genesis), (long) genesis.length, parentChainId, seg(parentGenesisHash)); }
    public byte[] blockHeader(byte[] block) { return call(blockHeaderH, seg(block), (long) block.length); }
    /** A header's 32-byte subchain root (zero below version 3). */
    public byte[] headerSubchainRoot(byte[] header) { return call(headerSubchainRootH, seg(header), (long) header.length); }
    public byte[] certifiedHeader(byte[] header, byte[] certificate) { return call(certifiedHeaderEncode, seg(header), (long) header.length, seg(certificate), (long) certificate.length); }
    /** An anchor: one child's CONSECUTIVE certified headers. */
    public byte[] encodeAnchor(long childChainId, byte[][] certifiedHeaders) { byte[] blob = blobList(certifiedHeaders); return call(anchorEncode, childChainId, seg(blob), (long) blob.length); }
    /** A cross-chain proof: hops (parent -> ... -> the chain holding the entry) + that chain's state proof. */
    public byte[] encodeSubchainProof(byte[][] hops, byte[] stateProof) { byte[] blob = blobList(hops); return call(subchainProofEncode, seg(blob), (long) blob.length, seg(stateProof), (long) stateProof.length); }
    /** Whether the proof is committed under the parent's 32-byte subchain root all the way down and its state proof holds. */
    public boolean verifySubchainProof(byte[] parentSubchainRoot, byte[] proof) {
        if (parentSubchainRoot.length != 32) return false;
        try { return (int) subchainProofVerify.invoke(seg(parentSubchainRoot), seg(proof), (long) proof.length) == 1; } catch (Throwable e) { throw new RuntimeException(e); }
    }
    // -- Foreign components (engines / proposers implemented in Java) ------------

    /** A consensus engine implemented in Java, plugged into a mixture by id. The
     *  consensus pipeline stays native; Java supplies the JUSTIFICATION policy.
     *  Both methods run synchronously inside consensus and MUST be deterministic
     *  (no clock, no randomness) and must not call back into the chain. */
    public interface ForeignEngine {
        /** The justification for a block this node proposed (<= 4096 B), or null to refuse. */
        byte[] justify(byte[] block);
        boolean verify(byte[] block, byte[] justification);
    }
    public interface ForeignProposer { int propose(long height, int validators); }

    private static final java.util.Map<Long, Object> FOREIGN = new java.util.concurrent.ConcurrentHashMap<>();
    private static final java.util.concurrent.atomic.AtomicLong NEXT_HANDLE = new java.util.concurrent.atomic.AtomicLong(1);

    @SuppressWarnings("unused")
    private static int justifyUpcall(MemorySegment ctx, MemorySegment block, long len, MemorySegment out, long cap, MemorySegment outLen) {
        Object o = FOREIGN.get(ctx.address());
        if (!(o instanceof ForeignEngine e)) return 0;
        byte[] j = e.justify(block.reinterpret(len).toArray(JAVA_BYTE));
        if (j == null || j.length > cap) return 0;
        MemorySegment.copy(j, 0, out.reinterpret(cap), JAVA_BYTE, 0, j.length);
        outLen.reinterpret(8).set(JAVA_LONG, 0, (long) j.length);
        return 1;
    }
    @SuppressWarnings("unused")
    private static int verifyUpcall(MemorySegment ctx, MemorySegment block, long len, MemorySegment just, long jlen) {
        Object o = FOREIGN.get(ctx.address());
        if (!(o instanceof ForeignEngine e)) return 0;
        return e.verify(block.reinterpret(len).toArray(JAVA_BYTE), just.reinterpret(jlen).toArray(JAVA_BYTE)) ? 1 : 0;
    }
    @SuppressWarnings("unused")
    private static int proposerUpcall(MemorySegment ctx, long height, int validators) {
        Object o = FOREIGN.get(ctx.address());
        return (o instanceof ForeignProposer p) ? p.propose(height, validators) : 0;
    }
    private MemorySegment upcall(String name, java.lang.invoke.MethodType type, FunctionDescriptor fd) {
        try {
            var mh = java.lang.invoke.MethodHandles.lookup().findStatic(Aviary.class, name, type);
            return java.lang.foreign.Linker.nativeLinker().upcallStub(mh, fd, arena);
        } catch (Throwable e) { throw new RuntimeException(e); }
    }

    /** Register a Java {@link ForeignEngine} under id (chain identity - never rename). */
    public boolean registerEngine(String id, ForeignEngine engine, boolean grindProof) {
        long h = NEXT_HANDLE.getAndIncrement();
        FOREIGN.put(h, engine);
        MemorySegment j = upcall("justifyUpcall",
                java.lang.invoke.MethodType.methodType(int.class, MemorySegment.class, MemorySegment.class, long.class, MemorySegment.class, long.class, MemorySegment.class),
                FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS));
        MemorySegment v = upcall("verifyUpcall",
                java.lang.invoke.MethodType.methodType(int.class, MemorySegment.class, MemorySegment.class, long.class, MemorySegment.class, long.class),
                FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS, JAVA_LONG, ADDRESS, JAVA_LONG));
        try { return (int) regAddEngine.invoke(arena.allocateFrom(id), j, v, MemorySegment.ofAddress(h), grindProof ? 1 : 0) == 1; }
        catch (Throwable e) { throw new RuntimeException(e); }
    }
    /** Register a Java proposer policy under id. Must be deterministic. */
    public boolean registerProposer(String id, ForeignProposer policy) {
        long h = NEXT_HANDLE.getAndIncrement();
        FOREIGN.put(h, policy);
        MemorySegment p = upcall("proposerUpcall",
                java.lang.invoke.MethodType.methodType(int.class, MemorySegment.class, long.class, int.class),
                FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_LONG, JAVA_INT));
        try { return (int) regAddProposer.invoke(arena.allocateFrom(id), p, MemorySegment.ofAddress(h)) == 1; }
        catch (Throwable e) { throw new RuntimeException(e); }
    }

    /** Assemble for validator me. Returns null on any fail-closed reject (unknown id, state mismatch, posture-forbidden engine). */
    public Chain assembleChain(byte[] genesis, int me, byte[] blsPub, byte[] blsSec) {
        try {
            MemorySegment h = (MemorySegment) chainAssemble.invoke(seg(genesis), (long) genesis.length, me, seg(blsPub), (long) blsPub.length, seg(blsSec), (long) blsSec.length);
            return h.address() == 0 ? null : new Chain(h);
        } catch (Throwable e) { throw new RuntimeException(e); }
    }

    public boolean verifyStateProof(byte[] root, byte[] proof) {
        try { return (int) verifyProof.invoke(seg(root), seg(proof), (long) proof.length) == 1; }
        catch (Throwable t) { throw new RuntimeException(t); }
    }

    public Node createNode(long chainId) {
        try {
            MemorySegment h = (MemorySegment) nodeNew.invoke(chainId);
            if (h.address() == 0) throw new IllegalStateException("node create failed");
            return new Node(h);
        } catch (Throwable t) { throw new RuntimeException(t); }
    }

    /** Create a custodial key store protected by {@code passphrase} (unlocked). */
    public KeyStore createKeyStore(String passphrase) {
        try {
            MemorySegment h = (MemorySegment) ksNew.invoke(arena.allocateFrom(passphrase));
            if (h.address() == 0) throw new IllegalStateException("keystore create failed");
            return new KeyStore(h);
        } catch (Throwable t) { throw new RuntimeException(t); }
    }

    /** Load a serialised key store. It comes back LOCKED - call unlock(). */
    public KeyStore loadKeyStore(byte[] blob) {
        try {
            MemorySegment h = (MemorySegment) ksLoad.invoke(seg(blob), (long) blob.length);
            if (h.address() == 0) throw new IllegalStateException("keystore load failed");
            return new KeyStore(h);
        } catch (Throwable t) { throw new RuntimeException(t); }
    }

    /** A custodial store that seals account signing secrets at rest. */
    public final class KeyStore implements AutoCloseable {
        private final MemorySegment ks;
        private KeyStore(MemorySegment ks) { this.ks = ks; }

        /** Seal an account's secret; returns its 32-byte address. Requires unlocked. */
        public byte[] addAccount(byte[] pub, byte[] secret) {
            try {
                MemorySegment out = arena.allocate(32);
                int rc = (int) ksAdd.invoke(ks, seg(pub), (long) pub.length, seg(secret),
                                            (long) secret.length, out);
                if (rc != 0) throw new IllegalStateException("keystore add account failed (locked?)");
                return out.reinterpret(32).toArray(JAVA_BYTE);
            } catch (Throwable t) { throw new RuntimeException(t); }
        }

        /** Decrypt the sealed secret for {@code address}. Requires unlocked. */
        public byte[] reveal(byte[] address) {
            try { return consume((MemorySegment) ksReveal.invoke(arena.allocate(RES), ks, seg(address))); }
            catch (Throwable t) { throw new RuntimeException(t); }
        }

        /** The persistable (encrypted) blob - no plaintext key. */
        public byte[] serialise() {
            try { return consume((MemorySegment) ksSerialise.invoke(arena.allocate(RES), ks)); }
            catch (Throwable t) { throw new RuntimeException(t); }
        }

        /** Recover the master key from {@code passphrase}. Throws on a wrong passphrase. */
        public void unlock(String passphrase) {
            try {
                if ((int) ksUnlock.invoke(ks, arena.allocateFrom(passphrase)) != 0) {
                    throw new IllegalStateException("keystore unlock failed");
                }
            } catch (Throwable t) { throw new RuntimeException(t); }
        }

        /** Signs a transfer from the stored account at from without revealing its secret; the scheme follows the stored key and a zero fee gives the version 1 envelope. Returns the encoded transaction. */
        public byte[] signTransfer(long chainId, byte[] from, byte[] to, long amount, long nonce, long fee) {
            return call(ksSign, ks, chainId, seg(from), seg(to), amount, nonce, fee);
        }
        /** The stored public key for address. */
        public byte[] publicKey(byte[] address) { return call(ksPublicKey, ks, seg(address)); }
        /** Whether the store holds address. */
        public boolean has(byte[] address) { try { return (int) ksHas.invoke(ks, seg(address)) == 1; } catch (Throwable t) { throw new RuntimeException(t); } }
        /** Every stored address. */
        public byte[][] addresses() { return splitBlobList(consume(invokeRes(ksAddresses, ks))); }
        /** Forgets the master key until the next unlock. */
        public void lock() { try { ksLock.invoke(ks); } catch (Throwable t) { throw new RuntimeException(t); } }
        /** Whether secrets can currently be revealed. */
        public boolean unlocked() {
            try { return (int) ksUnlocked.invoke(ks) == 1; } catch (Throwable t) { throw new RuntimeException(t); }
        }

        @Override public void close() { try { ksFree.invoke(ks); } catch (Throwable ignored) {} }
    }

    /** A running chain node. */
    public final class Node implements AutoCloseable {
        private final MemorySegment node;
        private long eventHandle;
        private Node(MemorySegment node) { this.node = node; }

        public Account newAccount() {
            byte[] secret = consume(invokeRes(newAccount, node));
            byte[] pub = java.util.Arrays.copyOfRange(secret, 32, 64);
            return new Account(secret, pub, accountAddress(pub));
        }
        /** Fresh hybrid (Ed25519 + ML-DSA-65) post-quantum account. */
        public Account newAccountHybrid() {
            byte[] blob = consume(invokeRes(newAccountHybrid, node));
            // [pub_len: 4 bytes big-endian][public][secret]
            int pl = ((blob[0] & 0xFF) << 24) | ((blob[1] & 0xFF) << 16) | ((blob[2] & 0xFF) << 8) | (blob[3] & 0xFF);
            byte[] pub = java.util.Arrays.copyOfRange(blob, 4, 4 + pl);
            byte[] sec = java.util.Arrays.copyOfRange(blob, 4 + pl, blob.length);
            return new Account(sec, pub, accountAddress(pub));
        }
        /** Build, sign, and admit a hybrid (post-quantum) transfer. */
        public byte[] submitTransferHybrid(byte[] secret, byte[] pub, byte[] to, long amount, long n) {
            try {
                return consume((MemorySegment) submitHybrid.invoke(arena.allocate(RES), node,
                    seg(secret), (long) secret.length, seg(pub), (long) pub.length, seg(to), amount, n));
            } catch (Throwable t) { throw new RuntimeException(t); }
        }
        public byte[] accountAddress(byte[] pub) {
            try { return consume((MemorySegment) addr.invoke(arena.allocate(RES), seg(pub), (long) pub.length)); }
            catch (Throwable t) { throw new RuntimeException(t); }
        }
        // Valid until the first block is produced (reads do NOT seal genesis);
        // throws afterwards - never a silent no-op.
        public void credit(byte[] address, long amount) {
            int rc;
            try { rc = (int) credit.invoke(node, seg(address), amount); }
            catch (Throwable t) { throw new RuntimeException(t); }
            if (rc == 2) throw new IllegalStateException(
                "genesis is sealed (a block was produced); fund accounts with transfers");
            if (rc != 0) throw new RuntimeException("credit failed (status " + rc + ")");
        }
        public byte[] submitTransfer(byte[] secret, byte[] to, long amount, long n) {
            try { return consume((MemorySegment) submit.invoke(arena.allocate(RES), node, seg(secret), (long) secret.length, seg(to), amount, n)); }
            catch (Throwable t) { throw new RuntimeException(t); }
        }
        /** Ed25519 transfer carrying a priority-fee tip (version-2 envelope) for fee-market priority. */
        public byte[] submitTransferFee(byte[] secret, byte[] to, long amount, long n, long fee) {
            try { return consume((MemorySegment) submitFee.invoke(arena.allocate(RES), node, seg(secret), (long) secret.length, seg(to), amount, n, fee)); }
            catch (Throwable t) { throw new RuntimeException(t); }
        }
        public long produceBlock(long ts) {
            try {
                MemorySegment out = arena.allocate(JAVA_LONG);
                if ((int) produce.invoke(node, ts, out) != 0) throw new IllegalStateException("produce failed");
                return out.get(JAVA_LONG, 0);
            } catch (Throwable t) { throw new RuntimeException(t); }
        }
        public long height()      { return u64(height); }
        public long mempoolSize() { return u64(mempool); }
        public long balance(byte[] a) { return u64a(balance, a); }
        public long nonce(byte[] a)   { return u64a(nonce, a); }
        /** The committed nonce advanced past every pending transfer from the address. */
        public long nextNonce(byte[] a) { return u64a(nodeNextNonce, a); }
        public byte[] stateRoot()  { return consume(invokeRes(stateRoot, node)); }
        public String drainEvents(){ return new String(consume(invokeRes(drain, node)), StandardCharsets.UTF_8); }
        public String metrics()    { return new String(consume(invokeRes(metrics, node)), StandardCharsets.UTF_8); }
        public byte[] proveAccount(byte[] a) {
            try { return consume((MemorySegment) prove.invoke(arena.allocate(RES), node, seg(a))); }
            catch (Throwable t) { throw new RuntimeException(t); }
        }
        /** The whole chain as one blob: chain id, genesis credits and every finalized block with its certificate. Pending transactions are not included. */
        public byte[] snapshot() { return consume(invokeRes(nodeSnapshot, node)); }
        /** The encoded finalized block at height (1 based). */
        public byte[] blockAt(long height) { return call(nodeBlockAt, node, height); }
        /** Admits an already signed, encoded transaction; returns its txid. */
        public byte[] submitTx(byte[] encoded) { return call(nodeSubmitTx, node, seg(encoded), (long) encoded.length); }
        /** Signs a transfer from the key store account at from and admits it; the secret never leaves the engine. */
        public byte[] submitTransferFromStore(KeyStore store, byte[] from, byte[] to, long amount, long nonce, long fee) {
            return call(nodeSubmitKs, node, store.ks, seg(from), seg(to), amount, nonce, fee);
        }
        /** Receives every chain event as one JSON object, synchronously from the node call that caused it; null removes the hook. While set, drainEvents is empty. */
        public void setEventHook(java.util.function.Consumer<String> hook) {
            try {
                if (eventHandle != 0) { NODE_EVENTS.remove(eventHandle); eventHandle = 0; }
                if (hook == null) { nodeSetEventHook.invoke(node, MemorySegment.NULL, MemorySegment.NULL); return; }
                eventHandle = NEXT_HANDLE.getAndIncrement();
                NODE_EVENTS.put(eventHandle, hook);
                nodeSetEventHook.invoke(node, nodeEventStub, MemorySegment.ofAddress(eventHandle));
            } catch (Throwable t) { throw new RuntimeException(t); }
        }
        private long u64(MethodHandle mh) { try { return (long) mh.invoke(node); } catch (Throwable t) { throw new RuntimeException(t); } }
        private long u64a(MethodHandle mh, byte[] a) { try { return (long) mh.invoke(node, seg(a)); } catch (Throwable t) { throw new RuntimeException(t); } }
        public void close() {
            try { if (eventHandle != 0) { nodeSetEventHook.invoke(node, MemorySegment.NULL, MemorySegment.NULL); NODE_EVENTS.remove(eventHandle); eventHandle = 0; } } catch (Throwable ignored) {}
            try { nodeFree.invoke(node); } catch (Throwable ignored) {}
        }
    }

    // -- Persistence, networking and secure sessions --
    /** Rebuilds a node from Node#snapshot, replaying every block through consensus. Throws on a malformed blob. */
    public Node restoreNode(byte[] snapshot) { return restoreNode(snapshot, false); }
    /** Like restoreNode; with replay every archived block goes through consensus again (slow; audits the archive). */
    public Node restoreNode(byte[] snapshot, boolean replay) {
        try {
            MemorySegment h = (MemorySegment) (replay ? nodeRestoreReplay : nodeRestore).invoke(seg(snapshot), (long) snapshot.length);
            if (h.address() == 0) throw new IllegalArgumentException("node restore refused (malformed snapshot)");
            return new Node(h);
        } catch (RuntimeException e) { throw e; } catch (Throwable t) { throw new RuntimeException(t); }
    }
    private static final java.util.Map<Long, java.util.function.Consumer<String>> NODE_EVENTS = new java.util.concurrent.ConcurrentHashMap<>();
    /** Points the native hook of handle h at hook (or clears it when null) and returns the new registry key, 0 when cleared. */
    private long hookEvents(MethodHandle setter, MemorySegment h, long previous, java.util.function.Consumer<String> hook) {
        try {
            if (previous != 0) NODE_EVENTS.remove(previous);
            if (hook == null) { if (previous != 0) setter.invoke(h, MemorySegment.NULL, MemorySegment.NULL); return 0; }
            long key = NEXT_HANDLE.getAndIncrement();
            NODE_EVENTS.put(key, hook);
            setter.invoke(h, nodeEventStub, MemorySegment.ofAddress(key));
            return key;
        } catch (Throwable t) { throw new RuntimeException(t); }
    }
    @SuppressWarnings("unused")
    private static void nodeEventUpcall(MemorySegment ctx, MemorySegment json, long len) {
        var c = NODE_EVENTS.get(ctx.address());
        if (c != null) c.accept(new String(json.reinterpret(len).toArray(JAVA_BYTE), StandardCharsets.UTF_8));
    }
    private final MemorySegment nodeEventStub = upcall("nodeEventUpcall",
            java.lang.invoke.MethodType.methodType(void.class, MemorySegment.class, MemorySegment.class, long.class),
            FunctionDescriptor.ofVoid(ADDRESS, ADDRESS, JAVA_LONG));
    static byte[][] splitBlobList(byte[] b) {
        java.nio.ByteBuffer bb = java.nio.ByteBuffer.wrap(b);
        int n = bb.getInt(); byte[][] out = new byte[n][];
        for (int i = 0; i < n; i++) { byte[] it = new byte[bb.getInt()]; bb.get(it); out[i] = it; }
        return out;
    }

    /** The validator index that follows a chain without voting. */
    public static final int NET_OBSERVER = 0xFFFFFFFF;
    public static final int STEP_PROPOSE = 0, STEP_PREVOTE = 1, STEP_PRECOMMIT = 2;
    /** A gossip message to send to one peer. */
    public record Relay(int to, byte[] message) {}
    /** A consensus timer to arm; when it fires, call NetNode#onTimeout. */
    public record TimeoutRequest(int step, int round) {}
    /** What a NetNode wants done after an input: relays to send and timers to arm (durations are the caller's choice). */
    public record Outbound(java.util.List<Relay> relays, java.util.List<TimeoutRequest> timeouts) {}
    /** A long term peer identity: hybrid KEM pair and hybrid signature pair. */
    public record SecureIdentity(PortableAccount kem, PortableAccount sig) {}

    static Outbound decodeOutbound(byte[] b) {
        java.nio.ByteBuffer bb = java.nio.ByteBuffer.wrap(b);
        var relays = new java.util.ArrayList<Relay>(); var timeouts = new java.util.ArrayList<TimeoutRequest>();
        int rc = bb.getInt();
        for (int i = 0; i < rc; i++) { int to = bb.getInt(); byte[] m = new byte[bb.getInt()]; bb.get(m); relays.add(new Relay(to, m)); }
        int tc = bb.getInt();
        for (int i = 0; i < tc; i++) { int step = bb.get() & 0xFF; int round = bb.getInt(); timeouts.add(new TimeoutRequest(step, round)); }
        return new Outbound(relays, timeouts);
    }

    /** A validator or observer running BFT consensus over a gossip mesh with no socket and no clock of its own: every input returns an Outbound the application acts on. */
    public final class NetNode implements AutoCloseable {
        private final MemorySegment h;
        private NetNode(MemorySegment h) { this.h = h; }
        public void addPeer(int peer) { try { netAddPeer.invoke(h, peer); } catch (Throwable t) { throw new RuntimeException(t); } }
        /** Encoded transactions this node proposes at the current height; cleared by advance. */
        public void setTransactions(byte[][] encodedTxs) {
            byte[] blob = blobList(encodedTxs);
            try { if ((int) netSetTxs.invoke(h, seg(blob), (long) blob.length) != 1) throw new IllegalArgumentException("net node: malformed transaction"); }
            catch (RuntimeException e) { throw e; } catch (Throwable t) { throw new RuntimeException(t); }
        }
        public Outbound start() { return decodeOutbound(consume(invokeRes(netStart, h))); }
        public Outbound onGossip(int from, byte[] payload) { return decodeOutbound(call(netOnGossip, h, from, seg(payload), (long) payload.length)); }
        public Outbound onTimeout(TimeoutRequest t) { return decodeOutbound(call(netOnTimeout, h, (byte) t.step(), t.round())); }
        public Outbound advance() { return decodeOutbound(consume(invokeRes(netAdvance, h))); }
        public boolean committed() { try { return (int) netCommitted.invoke(h) == 1; } catch (Throwable t) { throw new RuntimeException(t); } }
        public long height() { try { return (long) netHeight.invoke(h); } catch (Throwable t) { throw new RuntimeException(t); } }
        public boolean peerBanned(int peer) { try { return (int) netPeerBanned.invoke(h, peer) == 1; } catch (Throwable t) { throw new RuntimeException(t); } }
        public byte[][] replayLog() { return splitBlobList(consume(invokeRes(netReplayLog, h))); }
        public byte[] tip() { return consume(invokeRes(netTip, h)); }
        public byte[] stateRoot() { return consume(invokeRes(netStateRoot, h)); }
        public long balance(byte[] address) { try { return (long) netBalance.invoke(h, seg(address)); } catch (Throwable t) { throw new RuntimeException(t); } }
        public byte[] proveAccount(byte[] address) { return call(netProve, h, seg(address)); }
        private long eventHandle;
        /** Buffered events (a block_finalized per committed height) as a JSON array; empty while a hook is set. */
        public String drainEvents() { return new String(consume(invokeRes(netDrainEvents, h)), StandardCharsets.UTF_8); }
        /** Receives every event as one JSON object, synchronously from the input that caused it; null removes the hook. */
        public void setEventHook(java.util.function.Consumer<String> hook) { eventHandle = hookEvents(netSetEventHook, h, eventHandle, hook); }
        /** Prometheus text: chain height, blocks finalized, gossip received, relayed and dropped, timeouts fired, peers banned. */
        public String metrics() { return new String(consume(invokeRes(netMetrics, h)), StandardCharsets.UTF_8); }
        public void close() {
            eventHandle = hookEvents(netSetEventHook, h, eventHandle, null);
            try { netFree.invoke(h); } catch (Throwable ignored) {}
        }
    }
    /** A consensus node for validator me of genesis, or an observer when me is NET_OBSERVER (bls key may then be null). */
    public NetNode newNetNode(byte[] genesis, int me, PortableAccount blsKey, long timestamp) {
        byte[] pk = blsKey == null ? new byte[0] : blsKey.publicKey(), sk = blsKey == null ? new byte[0] : blsKey.secret();
        try {
            MemorySegment h = (MemorySegment) netNew.invoke(seg(genesis), (long) genesis.length, me, seg(pk), (long) pk.length, seg(sk), (long) sk.length, timestamp);
            if (h.address() == 0) throw new IllegalArgumentException("net node: malformed genesis or key");
            return new NetNode(h);
        } catch (RuntimeException e) { throw e; } catch (Throwable t) { throw new RuntimeException(t); }
    }

    /** A fresh long term identity. */
    public SecureIdentity generateSecureIdentity() {
        byte[][] p = splitBlobList(consume(invokePtrRes(secIdentity)));
        return new SecureIdentity(new PortableAccount(p[0], p[1]), new PortableAccount(p[2], p[3]));
    }
    private MemorySegment invokePtrRes(MethodHandle mh) { try { return (MemorySegment) mh.invoke(arena.allocate(RES)); } catch (Throwable t) { throw new RuntimeException(t); } }
    /** A confidential, mutually authenticated, post quantum hybrid link. Pin peerIdentity against the key you expect. */
    public final class SecureSession implements AutoCloseable {
        private final MemorySegment h;
        private SecureSession(MemorySegment h) { this.h = h; }
        /** The handshake message this side must send: the first message for an initiator, the reply for a responder. */
        public byte[] handshakeMessage() { return consume(invokeRes(secHandshake, h)); }
        /** Initiator only: consumes the responder's reply. Throws when the responder does not verify. */
        public void complete(byte[] reply) {
            try { if ((int) secComplete.invoke(h, seg(reply), (long) reply.length) != 1) throw new IllegalArgumentException("secure session: responder refused"); }
            catch (RuntimeException e) { throw e; } catch (Throwable t) { throw new RuntimeException(t); }
        }
        public boolean open() { try { return (int) secOpen.invoke(h) == 1; } catch (Throwable t) { throw new RuntimeException(t); } }
        public byte[] peerIdentity() { return consume(invokeRes(secPeer, h)); }
        public byte[] encrypt(byte[] plaintext) { return call(secEncrypt, h, seg(plaintext), (long) plaintext.length); }
        public byte[] decrypt(byte[] frame) { return call(secDecrypt, h, seg(frame), (long) frame.length); }
        public void close() { try { secFree.invoke(h); } catch (Throwable ignored) {} }
    }
    /** Dialing side: encapsulates to the responder's KEM public key and authenticates with sig. */
    public SecureSession initiateSecureSession(byte[] responderKemPublic, PortableAccount sig) {
        try {
            MemorySegment h = (MemorySegment) secInitiate.invoke(seg(responderKemPublic), (long) responderKemPublic.length,
                    seg(sig.publicKey()), (long) sig.publicKey().length, seg(sig.secret()), (long) sig.secret().length);
            if (h.address() == 0) throw new IllegalArgumentException("secure session: initiate failed");
            return new SecureSession(h);
        } catch (RuntimeException e) { throw e; } catch (Throwable t) { throw new RuntimeException(t); }
    }
    /** Accepting side: consumes the initiator's first message under me; open at once. */
    public SecureSession acceptSecureSession(SecureIdentity me, byte[] firstMessage) {
        try {
            MemorySegment h = (MemorySegment) secAccept.invoke(
                    seg(me.kem().publicKey()), (long) me.kem().publicKey().length, seg(me.kem().secret()), (long) me.kem().secret().length,
                    seg(me.sig().publicKey()), (long) me.sig().publicKey().length, seg(me.sig().secret()), (long) me.sig().secret().length,
                    seg(firstMessage), (long) firstMessage.length);
            if (h.address() == 0) throw new IllegalArgumentException("secure session: initiator refused");
            return new SecureSession(h);
        } catch (RuntimeException e) { throw e; } catch (Throwable t) { throw new RuntimeException(t); }
    }

    // -- FFM plumbing ---------------------------------------------------------
    private MethodHandle h(String name, FunctionDescriptor fd) {
        return linker.downcallHandle(lib.find(name).orElseThrow(
                () -> new IllegalStateException("symbol not found: " + name)), fd);
    }
    private MemorySegment seg(byte[] b) {
        MemorySegment s = arena.allocate(b.length == 0 ? 1 : b.length);
        MemorySegment.copy(b, 0, s, JAVA_BYTE, 0, b.length);
        return s;
    }
    private MemorySegment invokePtr(MethodHandle mh) { try { return (MemorySegment) mh.invoke(); } catch (Throwable t) { throw new RuntimeException(t); } }
    private MemorySegment invokeRes(MethodHandle mh, MemorySegment node) {
        try { return (MemorySegment) mh.invoke(arena.allocate(RES), node); } catch (Throwable t) { throw new RuntimeException(t); }
    }
    private byte[] call1(MethodHandle mh, byte[] in) {
        try { return consume((MemorySegment) mh.invoke(arena.allocate(RES), seg(in))); } catch (Throwable t) { throw new RuntimeException(t); }
    }
    private String cstr(MemorySegment p) { return p.reinterpret(Long.MAX_VALUE).getString(0); }

    private byte[] consume(MemorySegment res) {
        MemorySegment data = res.get(ADDRESS, 0);
        long len = res.get(JAVA_LONG, ADDRESS.byteSize());
        MemorySegment err = res.get(ADDRESS, RES.byteOffset(MemoryLayout.PathElement.groupElement("error")));
        if ((data.address() == 0 || len == 0) && err.address() != 0) {
            String msg = err.reinterpret(Long.MAX_VALUE).getString(0);
            try { strFree.invoke(err); } catch (Throwable ignored) {}
            throw new RuntimeException(msg);
        }
        byte[] out = (data.address() == 0 || len == 0) ? new byte[0] : data.reinterpret(len).toArray(JAVA_BYTE);
        try {
            MemorySegment bufCopy = arena.allocate(BUF);
            MemorySegment.copy(res, 0, bufCopy, 0, B);
            bufFree.invoke(bufCopy);
        } catch (Throwable t) { throw new RuntimeException(t); }
        return out;
    }

    public void close() { arena.close(); }
}
